/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_badges_bot.h"

#include "staticgram/sg_badges.h"
#include "staticgram/sg_frame_params.h"
#include "apiwrap.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_app_config.h"
#include "main/main_session.h"

namespace StaticGram {
namespace {

constexpr auto kFramesQueryMinInterval = crl::time(5000);

struct AllowedFrames {
	std::optional<QStringList> styles;
	crl::time lastQuery = 0;
};

[[nodiscard]] QHash<uint64, AllowedFrames> &AllowedFramesMap() {
	static auto result = QHash<uint64, AllowedFrames>();
	return result;
}

[[nodiscard]] uint64 SelfId(not_null<Main::Session*> session) {
	return uint64(session->userId().bare);
}

[[nodiscard]] DocumentId ConfigDocumentId(
		not_null<Main::Session*> session,
		const QString &key) {
	// Document ids are strings: TL doubles lose int64 precision.
	return session->appConfig().get<QString>(key, QString()).toULongLong();
}

[[nodiscard]] QString ConfigString(
		not_null<Main::Session*> session,
		const QString &key,
		const QString &fallback = QString()) {
	const auto value = session->appConfig().get<QString>(
		key,
		QString()).trimmed();
	return value.isEmpty() ? fallback : value;
}

void SendInlineQuery(
		not_null<Main::Session*> session,
		not_null<UserData*> bot,
		const QString &query,
		Fn<void(QString)> done) {
	const auto weak = base::make_weak(session);
	session->api().request(MTPmessages_GetInlineBotResults(
		MTP_flags(0),
		bot->inputUser(),
		MTP_inputPeerEmpty(),
		MTPInputGeoPoint(),
		MTP_string(query),
		MTP_string()
	)).done([=](const MTPmessages_BotResults &result) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		auto text = QString();
		result.match([&](const MTPDmessages_botResults &data) {
			strong->data().processUsers(data.vusers());
			const auto &list = data.vresults().v;
			if (list.isEmpty()) {
				return;
			}
			list.front().match([&](const auto &entry) {
				entry.vsend_message().match([&](
						const MTPDbotInlineMessageText &message) {
					text = qs(message.vmessage());
				}, [&](const MTPDbotInlineMessageMediaAuto &message) {
					text = qs(message.vmessage());
				}, [](const auto &) {
				});
			});
		});
		done(text.trimmed());
	}).fail([=](const MTP::Error &error) {
		done(QString());
	}).send();
}

} // namespace

BadgesConfig ReadBadgesConfig(not_null<Main::Session*> session) {
	auto bot = ConfigString(session, u"sg_badges_bot"_q);
	if (bot.startsWith('@')) {
		bot = bot.mid(1);
	}
	return {
		.apiUrl = ConfigString(session, u"sg_badges_api_url"_q),
		.bot = bot,
		.shopUrl = ConfigString(session, u"sg_badges_shop_url"_q),
		.priceSupporter = ConfigString(
			session,
			u"sg_badges_price_supporter"_q,
			QString::fromUtf8("100 \xE2\x82\xBD / 100 \xE2\xAD\x90")),
		.priceCustom = ConfigString(
			session,
			u"sg_badges_price_custom"_q,
			QString::fromUtf8("300 \xE2\x82\xBD / 300 \xE2\xAD\x90")),
		.priceUpgrade = ConfigString(
			session,
			u"sg_badges_price_upgrade"_q,
			QString::fromUtf8("200 \xE2\x82\xBD")),
		.defaultDocumentId = ConfigDocumentId(
			session,
			u"sg_badges_default_document_id"_q),
		.developerDocumentId = ConfigDocumentId(
			session,
			u"sg_badges_developer_document_id"_q),
		.transfer = session->appConfig().get<bool>(
			u"sg_badges_transfer"_q,
			false),
	};
}

void TrackBadgesSession(not_null<Main::Session*> session) {
	session->appConfig().value(
	) | rpl::on_next([=] {
		SetBadgesApiUrl(ReadBadgesConfig(session).apiUrl);
	}, session->lifetime());
	session->appConfig().refreshed(
	) | rpl::on_next([=] {
		SyncBadgesNow();
		RefreshAllowedFrames(session);
	}, session->lifetime());
}

void WithBadgesBot(
		not_null<Main::Session*> session,
		Fn<void(UserData*)> done) {
	const auto username = ReadBadgesConfig(session).bot;
	if (username.isEmpty()) {
		done(nullptr);
		return;
	}
	if (const auto peer = session->data().peerByUsername(username)) {
		if (const auto user = peer->asUser()) {
			done(user);
			return;
		}
	}
	const auto weak = base::make_weak(session);
	session->api().request(MTPcontacts_ResolveUsername(
		MTP_flags(0),
		MTP_string(username),
		MTP_string()
	)).done([=](const MTPcontacts_ResolvedPeer &result) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		auto bot = (UserData*)nullptr;
		result.match([&](const MTPDcontacts_resolvedPeer &data) {
			strong->data().processUsers(data.vusers());
			strong->data().processChats(data.vchats());
			bot = strong->data().userLoaded(
				peerToUser(peerFromMTP(data.vpeer())));
		});
		done(bot);
	}).fail([=](const MTP::Error &error) {
		done(nullptr);
	}).send();
}

void SendBadgesBotRequest(
		not_null<Main::Session*> session,
		const QString &query,
		Fn<void(QString)> done) {
	const auto weak = base::make_weak(session);
	WithBadgesBot(session, [=](UserData *bot) {
		const auto strong = weak.get();
		if (!strong || !bot) {
			done(QString());
			return;
		}
		SendInlineQuery(strong, bot, query, done);
	});
}

void ApplyBadge(
		not_null<Main::Session*> session,
		DocumentId documentId,
		const QString &text,
		Fn<void(QString)> done) {
	const auto trimmed = text.trimmed();
	const auto query = u"badge "_q
		+ QString::number(documentId)
		+ (trimmed.isEmpty() ? QString() : (' ' + trimmed));
	const auto selfId = SelfId(session);
	SendBadgesBotRequest(session, query, [=](QString result) {
		if (result == u"ok"_q) {
			UpdateLocalBadge(selfId, documentId, trimmed);
			SyncBadgesNow();
		}
		done(result);
	});
}

void ApplyAvatarFrame(
		not_null<Main::Session*> session,
		std::optional<AvatarFrame> frame,
		Fn<void(QString)> done) {
	if (frame && !IsKnownAvatarFrameStyle(frame->style)) {
		frame = std::nullopt;
	}
	if (frame) {
		frame->params = FrameParams::NonDefault(frame->style, frame->params);
	}
	const auto query = frame
		? FrameParams::ToCommand(frame->style, frame->params)
		: u"frame none"_q;
	const auto selfId = SelfId(session);
	const auto weak = base::make_weak(session);
	SendBadgesBotRequest(session, query, [=](QString result) {
		if (result == u"ok"_q) {
			UpdateLocalFrame(selfId, frame);
			SyncBadgesNow();
		} else if (const auto strong = weak.get()) {
			RefreshAllowedFrames(strong);
		}
		done(result);
	});
}

void ApplyBanner(
		not_null<Main::Session*> session,
		DocumentId documentId,
		uint64 accessHash,
		bool video,
		Fn<void(QString)> done) {
	const auto query = documentId
		? (u"banner "_q
			+ QString::number(qint64(documentId))
			+ ' '
			+ QString::number(qint64(accessHash)))
		: u"banner none"_q;
	const auto selfId = SelfId(session);
	SendBadgesBotRequest(session, query, [=](QString result) {
		if (result == u"ok"_q) {
			UpdateLocalBanner(
				selfId,
				documentId
					? ProfileBanner{
						.id = QString::number(qint64(documentId)),
						.video = video,
					}
					: ProfileBanner());
			SyncBadgesNow();
		}
		done(result);
	});
}

void RefreshAllowedFrames(
		not_null<Main::Session*> session,
		Fn<void()> done) {
	const auto selfId = SelfId(session);
	auto &entry = AllowedFramesMap()[selfId];
	const auto now = crl::now();
	if (!done
		&& entry.lastQuery
		&& now - entry.lastQuery < kFramesQueryMinInterval) {
		return;
	}
	entry.lastQuery = now;
	SendBadgesBotRequest(session, u"frames"_q, [=](QString result) {
		if (!result.isEmpty()) {
			auto styles = QStringList();
			if (!result.startsWith(u"error"_q)) {
				for (const auto &part : result.split(',')) {
					const auto style = part.trimmed();
					if (IsKnownAvatarFrameStyle(style)) {
						styles.push_back(style);
					}
				}
			}
			const auto changed = (AllowedFramesMap()[selfId].styles
				!= std::make_optional(styles));
			AllowedFramesMap()[selfId].styles = std::move(styles);
			if (changed) {
				NotifyBadgesChanged();
			}
		}
		if (done) {
			done();
		}
	});
}

bool AllowedFramesKnown(not_null<Main::Session*> session) {
	const auto i = AllowedFramesMap().constFind(SelfId(session));
	return (i != AllowedFramesMap().cend()) && i->styles.has_value();
}

bool CanUseFrames(not_null<Main::Session*> session) {
	const auto i = AllowedFramesMap().constFind(SelfId(session));
	if (i != AllowedFramesMap().cend() && i->styles) {
		return !i->styles->isEmpty();
	}
	return BadgeOfUser(SelfId(session)).documentId != 0;
}

bool CanUseFrame(
		not_null<Main::Session*> session,
		const QString &style) {
	if (!CanUseFrames(session)) {
		return false;
	}
	const auto i = AllowedFramesMap().constFind(SelfId(session));
	return (i == AllowedFramesMap().cend())
		|| !i->styles
		|| i->styles->contains(style);
}

} // namespace StaticGram
