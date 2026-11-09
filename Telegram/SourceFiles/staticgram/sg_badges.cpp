/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_badges.h"

#include "data/data_peer.h"
#include "settings.h"
#include "ui/effects/animation_value.h"
#include "ui/power_saving.h"
#include "styles/style_basic.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

namespace StaticGram {
namespace {

constexpr auto kRefreshInterval = crl::time(60 * 1000);
constexpr auto kRetryInterval = crl::time(60 * 1000);
constexpr auto kForcedSyncMinInterval = crl::time(15 * 1000);
constexpr auto kProfileOpenMinAge = crl::time(30 * 1000);
constexpr auto kMaxResponseSize = qint64(16 * 1024 * 1024);

[[nodiscard]] QString DefaultApiUrl() {
	return u"https://badges.staticgram.top/api/v1/"_q;
}

[[nodiscard]] QString CachePath() {
	return cWorkingDir() + u"tdata/sg_badges.json"_q;
}

struct UserProfile {
	UserBadge badge;

	friend inline bool operator==(
			const UserProfile &a,
			const UserProfile &b) {
		return (a.badge.documentId == b.badge.documentId)
			&& (a.badge.text == b.badge.text)
			&& (a.badge.canChange == b.badge.canChange)
			&& (a.badge.developer == b.badge.developer)
			&& (a.badge.number == b.badge.number);
	}
	friend inline bool operator!=(
			const UserProfile &a,
			const UserProfile &b) {
		return !(a == b);
	}
};

class Store final {
public:
	Store();

	[[nodiscard]] DocumentId badge(not_null<PeerData*> peer);
	[[nodiscard]] UserBadge userBadge(uint64 userId);
	[[nodiscard]] const AvatarFrame *frame(uint64 userId);
	[[nodiscard]] ProfileBanner banner(uint64 userId);
	[[nodiscard]] QString apiBase() const;
	[[nodiscard]] rpl::producer<> updated() const {
		return _updated.events();
	}

	void setApiUrl(const QString &url);
	void syncNow(Fn<void()> done);
	void syncOnProfileOpen();
	void updateLocalBadge(uint64 userId, DocumentId id, QString text);
	void updateLocalFrame(uint64 userId, std::optional<AvatarFrame> frame);
	void updateLocalBanner(uint64 userId, ProfileBanner banner);
	void moveLocalBadge(uint64 fromUserId, uint64 toUserId);
	void notify();

private:
	// Returns std::nullopt when data is not a profile list,
	// otherwise whether anything changed.
	std::optional<bool> apply(const QByteArray &data);
	void refreshIfNeeded();
	void fetch();
	void finish();
	[[nodiscard]] QString listUrl() const;

	QHash<uint64, UserProfile> _users;
	QHash<uint64, DocumentId> _chatBadges;
	QHash<uint64, AvatarFrame> _frames;
	QHash<uint64, ProfileBanner> _banners;
	std::unique_ptr<QNetworkAccessManager> _network;
	std::vector<Fn<void()>> _done;
	QString _apiUrl;
	QByteArray _etag;
	crl::time _nextFetch = 0;
	crl::time _lastSuccess = 0;
	crl::time _lastForced = 0;
	bool _fetching = false;
	rpl::event_stream<> _updated;

};

Store::Store() {
	auto file = QFile(CachePath());
	if (file.open(QIODevice::ReadOnly)) {
		const auto data = file.read(kMaxResponseSize);
		file.close();
		[[maybe_unused]] const auto result = apply(data);
	}
}

QString Store::apiBase() const {
	auto base = _apiUrl.isEmpty() ? DefaultApiUrl() : _apiUrl;
	if (!base.endsWith('/')) {
		base += '/';
	}
	return base;
}

QString Store::listUrl() const {
	return apiBase() + u"profiles"_q;
}

void Store::setApiUrl(const QString &url) {
	const auto trimmed = url.trimmed();
	if (!trimmed.startsWith(u"https://"_q) || trimmed == _apiUrl) {
		return;
	}
	const auto first = _apiUrl.isEmpty();
	_apiUrl = trimmed;
	if (!first) {
		_etag = QByteArray();
	}
}

std::optional<bool> Store::apply(const QByteArray &data) {
	// documentId values do not fit into a double, keep them as strings.
	static const auto kBigNumbers = QRegularExpression(
		u"(?<!\\\\)\"(documentId|id)\"\\s*:\\s*(-?\\d+)"_q);
	auto text = QString::fromUtf8(data);
	text.replace(kBigNumbers, u"\"\\1\":\"\\2\""_q);
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(text.toUtf8(), &error);
	if (error.error != QJsonParseError::NoError || !document.isArray()) {
		return std::nullopt;
	}
	auto users = QHash<uint64, UserProfile>();
	auto chatBadges = QHash<uint64, DocumentId>();
	auto frames = QHash<uint64, AvatarFrame>();
	auto banners = QHash<uint64, ProfileBanner>();
	for (const auto &value : document.array()) {
		const auto profile = value.toObject();
		if (profile.value(u"deleted"_q).toBool()) {
			continue;
		}
		auto ok = false;
		const auto id = profile.value(u"id"_q).toString().toLongLong(&ok);
		if (!ok || !id) {
			continue;
		}
		const auto bare = uint64(id < 0 ? -id : id);
		const auto chat = (profile.value(u"type"_q).toString() == u"CHAT"_q);
		const auto badge = profile.value(u"badge"_q).toObject();
		const auto documentId = badge.value(
			u"documentId"_q).toString().toULongLong(&ok);
		if (ok && documentId) {
			if (chat) {
				chatBadges.insert(bare, documentId);
			} else {
				const auto status = profile.value(u"status"_q).toString();
				// Android BadgesController.getNumber: supporters only.
				const auto supporter = (status == u"SUPPORTER"_q)
					|| (status == u"DEVELOPER"_q);
				const auto number = profile.value(u"number"_q).toInt();
				users.insert(bare, UserProfile{ UserBadge{
					.documentId = documentId,
					.text = badge.value(u"text"_q).toString(),
					.canChange = profile.value(
						u"canChangeBadge"_q).toBool(),
					.developer = (status == u"DEVELOPER"_q),
					.number = (supporter && number > 0) ? number : 0,
				} });
			}
		}
		const auto style = profile.value(u"frame"_q).toString();
		if (!chat && IsKnownAvatarFrameStyle(style)) {
			auto frame = AvatarFrame{ .style = style };
			const auto params = profile.value(u"frame_params"_q).toObject();
			for (auto i = params.begin(); i != params.end(); ++i) {
				if (i.value().isString()) {
					frame.params.insert(i.key(), i.value().toString());
				}
			}
			frames.insert(bare, std::move(frame));
		}
		const auto bannerId = profile.value(u"banner_id"_q).toString();
		const auto bannerType = profile.value(u"banner_type"_q).toString();
		auto bannerOk = false;
		const auto bannerNumber = bannerId.toLongLong(&bannerOk);
		if (!chat
			&& bannerOk
			&& bannerNumber != 0
			&& (bannerType == u"photo"_q || bannerType == u"video"_q)) {
			banners.insert(bare, ProfileBanner{
				.id = QString::number(bannerNumber),
				.video = (bannerType == u"video"_q),
			});
		}
	}
	const auto changed = (users != _users)
		|| (chatBadges != _chatBadges)
		|| (frames != _frames)
		|| (banners != _banners);
	_users = std::move(users);
	_chatBadges = std::move(chatBadges);
	_frames = std::move(frames);
	_banners = std::move(banners);
	return changed;
}

void Store::refreshIfNeeded() {
	const auto now = crl::now();
	if (_fetching || now < _nextFetch) {
		return;
	}
	_fetching = true;
	_nextFetch = now + kRefreshInterval;
	// Usually asked from a paint event, start the request after it.
	QTimer::singleShot(0, [=] { fetch(); });
}

void Store::syncNow(Fn<void()> done) {
	if (done) {
		_done.push_back(std::move(done));
	}
	if (_fetching) {
		return;
	}
	const auto now = crl::now();
	if (_lastForced && now - _lastForced < kForcedSyncMinInterval) {
		finish();
		return;
	}
	_lastForced = now;
	_fetching = true;
	_nextFetch = now + kRefreshInterval;
	QTimer::singleShot(0, [=] { fetch(); });
}

void Store::syncOnProfileOpen() {
	const auto now = crl::now();
	if (_fetching
		|| (_lastSuccess && now - _lastSuccess < kProfileOpenMinAge)
		|| (!_lastSuccess && _nextFetch && now < _nextFetch)) {
		return;
	}
	_fetching = true;
	_nextFetch = now + kRefreshInterval;
	QTimer::singleShot(0, [=] { fetch(); });
}

void Store::finish() {
	auto done = base::take(_done);
	for (const auto &callback : done) {
		callback();
	}
}

void Store::fetch() {
	if (!_network) {
		_network = std::make_unique<QNetworkAccessManager>();
	}
	auto request = QNetworkRequest(QUrl(listUrl()));
	request.setRawHeader("Accept", "application/json");
	if (!_etag.isEmpty()) {
		request.setRawHeader("If-None-Match", _etag);
	}
	request.setAttribute(
		QNetworkRequest::CacheLoadControlAttribute,
		QNetworkRequest::AlwaysNetwork);
	request.setTransferTimeout(15000);
	const auto reply = _network->get(request);
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		reply->deleteLater();
		_fetching = false;
		const auto status = reply->attribute(
			QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (status == 304) {
			_lastSuccess = crl::now();
			finish();
			return;
		} else if (reply->error() != QNetworkReply::NoError
			|| status != 200) {
			_nextFetch = crl::now() + kRetryInterval;
			finish();
			return;
		}
		const auto data = reply->read(kMaxResponseSize);
		const auto result = apply(data);
		if (!result) {
			_nextFetch = crl::now() + kRetryInterval;
			finish();
			return;
		}
		_lastSuccess = crl::now();
		_etag = reply->rawHeader("ETag");
		if (*result) {
			QDir().mkpath(cWorkingDir() + u"tdata"_q);
			auto file = QFile(CachePath());
			if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				file.write(data);
				file.close();
			}
			notify();
		}
		finish();
	});
}

void Store::notify() {
	_updated.fire({});
	for (const auto widget : QApplication::allWidgets()) {
		widget->update();
	}
}

void Store::updateLocalBadge(uint64 userId, DocumentId id, QString text) {
	auto &profile = _users[userId];
	if (profile.badge.documentId == id && profile.badge.text == text) {
		return;
	}
	profile.badge.documentId = id;
	profile.badge.text = std::move(text);
	_etag = QByteArray();
	notify();
}

void Store::updateLocalFrame(
		uint64 userId,
		std::optional<AvatarFrame> frame) {
	if (frame && IsKnownAvatarFrameStyle(frame->style)) {
		_frames[userId] = std::move(*frame);
	} else {
		_frames.remove(userId);
	}
	_etag = QByteArray();
	notify();
}

void Store::updateLocalBanner(uint64 userId, ProfileBanner banner) {
	if (banner) {
		_banners[userId] = std::move(banner);
	} else {
		_banners.remove(userId);
	}
	_etag = QByteArray();
	notify();
}

void Store::moveLocalBadge(uint64 fromUserId, uint64 toUserId) {
	const auto i = _users.constFind(fromUserId);
	if (i == _users.cend() || fromUserId == toUserId) {
		return;
	}
	auto profile = i.value();
	_users.remove(fromUserId);
	_users.insert(toUserId, std::move(profile));
	_etag = QByteArray();
	notify();
}

ProfileBanner Store::banner(uint64 userId) {
	refreshIfNeeded();
	return _banners.value(userId);
}

DocumentId Store::badge(not_null<PeerData*> peer) {
	refreshIfNeeded();
	const auto bare = uint64(peer->id.value & PeerId::kChatTypeMask);
	if (peer->isUser()) {
		const auto i = _users.constFind(bare);
		return (i != _users.cend()) ? i->badge.documentId : DocumentId();
	} else if (peer->isChat() || peer->isChannel()) {
		return _chatBadges.value(bare, 0);
	}
	return 0;
}

UserBadge Store::userBadge(uint64 userId) {
	refreshIfNeeded();
	const auto i = _users.constFind(userId);
	return (i != _users.cend()) ? i->badge : UserBadge();
}

const AvatarFrame *Store::frame(uint64 userId) {
	refreshIfNeeded();
	const auto i = _frames.constFind(userId);
	return (i != _frames.cend()) ? &i.value() : nullptr;
}

[[nodiscard]] Store &Instance() {
	// Never destroyed: it may be used until the very end of the app.
	static const auto instance = new Store();
	return *instance;
}

} // namespace

DocumentId BadgeDocumentId(not_null<PeerData*> peer) {
	return Instance().badge(peer);
}

ProfileBanner BannerOfUser(uint64 userId) {
	return Instance().banner(userId);
}

QString BadgesApiBase() {
	return Instance().apiBase();
}

UserBadge BadgeOfUser(uint64 userId) {
	return Instance().userBadge(userId);
}

const AvatarFrame *PeerAvatarFrame(not_null<PeerData*> peer) {
	if (!peer->isUser()) {
		return nullptr;
	}
	return Instance().frame(uint64(peer->id.value & PeerId::kChatTypeMask));
}

const AvatarFrame *UserAvatarFrame(uint64 userId) {
	return Instance().frame(userId);
}

rpl::producer<> BadgesUpdated() {
	return Instance().updated();
}

void NotifyBadgesChanged() {
	Instance().notify();
}

void SetBadgesApiUrl(const QString &url) {
	Instance().setApiUrl(url);
}

void SyncBadgesNow(Fn<void()> done) {
	Instance().syncNow(std::move(done));
}

void SyncBadgesOnProfileOpen() {
	Instance().syncOnProfileOpen();
}

void UpdateLocalBadge(uint64 userId, DocumentId documentId, QString text) {
	Instance().updateLocalBadge(userId, documentId, std::move(text));
}

void UpdateLocalFrame(uint64 userId, std::optional<AvatarFrame> frame) {
	Instance().updateLocalFrame(userId, std::move(frame));
}

void UpdateLocalBanner(uint64 userId, ProfileBanner banner) {
	Instance().updateLocalBanner(userId, std::move(banner));
}

void MoveLocalBadge(uint64 fromUserId, uint64 toUserId) {
	Instance().moveLocalBadge(fromUserId, toUserId);
}

bool AvatarFramesAnimated() {
	return !anim::Disabled()
		&& !PowerSaving::On(PowerSaving::kAnimations)
		&& !PowerSaving::On(PowerSaving::kEmojiStatus);
}

bool AvatarFramesLight() {
	return st::windowBg->c.lightnessF() > 0.5;
}

void PaintPeerAvatarFrame(
		QPainter &p,
		PeerData *peer,
		QRectF photo,
		QRectF allowed,
		double opacity) {
	if (!peer || peer->hasActiveStories()) {
		return;
	}
	if (const auto frame = PeerAvatarFrame(peer)) {
		PaintAvatarFrame(
			p,
			*frame,
			photo,
			allowed,
			opacity,
			AvatarFramesAnimated(),
			AvatarFramesLight());
	}
}

} // namespace StaticGram
