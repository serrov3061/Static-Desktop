/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_profile_music_plate.h"

#include "api/api_peer_colors.h"
#include "apiwrap.h"
#include "data/data_changes.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_emoji_statuses.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_saved_music.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "media/audio/media_audio.h"
#include "media/player/media_player_instance.h"
#include "ui/abstract_button.h"
#include "ui/emoji_config.h"
#include "ui/image/image.h"
#include "ui/painter.h"
#include "ui/text/format_song_document_name.h"
#include "ui/text/text_custom_emoji.h"
#include "ui/top_background_gradient.h"
#include "window/window_session_controller.h"
#include "styles/style_chat_style.h"
#include "styles/style_widgets.h"

namespace StaticGram {
namespace {

// Android NowPlayingCard: 12dp padding, 64dp cover, 16sp / 14sp texts.
constexpr auto kPadding = 12;
constexpr auto kCover = 64;
constexpr auto kCoverRadius = 12;
constexpr auto kRadius = 22;
constexpr auto kPlayButton = 40;

// exteraGram UIUtil.nowPlayingPattern: x from the right edge and y from
// the vertical centre (dp), the emoji size (dp) and the opacity.
constexpr float64 kPattern[][4] = {
	{ -5.5, 20., 20., 0.35 },
	{ -5.5, -20., 20., 0.35 },
	{ -36., -42., 22., 0.375 },
	{ -36., 0., 25., 0.425 },
	{ -36., 42., 22., 0.375 },
	{ -70., 22., 23., 0.35 },
	{ -70., -22., 23., 0.35 },
	{ -99., 46., 21., 0.275 },
	{ -99., 0., 22., 0.325 },
	{ -99., -46., 21., 0.275 },
	{ -128., -23., 20., 0.225 },
	{ -128., 23., 20., 0.225 },
};
constexpr auto kPatternWidth = 140.;

[[nodiscard]] int Px(float64 value) {
	return style::ConvertScale(int(std::round(value)));
}

[[nodiscard]] rpl::producer<HistoryItem*> FirstSavedMusicValue(
		not_null<UserData*> user) {
	const auto savedMusic = &user->owner().savedMusic();
	const auto peerId = user->id;
	return rpl::single(
		peerId
	) | rpl::then(
		savedMusic->changed() | rpl::filter(rpl::mappers::_1 == peerId)
	) | rpl::map([=]() -> HistoryItem* {
		const auto &list = savedMusic->list(peerId);
		for (const auto &item : list) {
			const auto media = item->media();
			if (media && media->document()) {
				return item.get();
			}
		}
		return nullptr;
	}) | rpl::distinct_until_changed();
}

class Plate final : public Ui::AbstractButton {
public:
	Plate(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user);

	void setItem(HistoryItem *item);

protected:
	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;
	void onStateChanged(State was, StateChangeSource source) override;

private:
	struct Colors {
		std::optional<QColor> center;
		std::optional<QColor> edge;
		QColor pattern;
		DocumentId patternId = 0;
	};

	[[nodiscard]] Colors computeColors() const;
	void refreshColors();
	void refreshPattern(DocumentId id);
	void validateBackground();
	void validateCover();
	void paintPattern(QPainter &p);
	void paintCover(QPainter &p, QRect rect);
	void paintTexts(QPainter &p, QRect rect);
	[[nodiscard]] bool playing() const;
	void clicked();

	const not_null<Window::SessionController*> _controller;
	const not_null<UserData*> _user;
	HistoryItem *_item = nullptr;
	DocumentData *_document = nullptr;
	std::shared_ptr<Data::DocumentMedia> _media;
	QString _title;
	QString _performer;

	Colors _colors;
	QImage _background;
	QImage _cover;
	bool _coverGood = false;
	std::unique_ptr<Ui::Text::CustomEmoji> _patternEmoji;
	DocumentId _patternEmojiId = 0;
	base::flat_map<float64, QImage> _patternCache;
	std::vector<Ui::PatternPoint> _patternPoints;
	QSize _patternFor;
	bool _playing = false;
	rpl::lifetime _downloadLifetime;

};

Plate::Plate(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<UserData*> user)
: AbstractButton(parent)
, _controller(controller)
, _user(user) {
	setCursor(style::cur_pointer);
	setClickedCallback([=] { clicked(); });

	refreshColors();
	_user->session().changes().peerFlagsValue(
		_user,
		(Data::PeerUpdate::Flag::EmojiStatus
			| Data::PeerUpdate::Flag::ColorProfile)
	) | rpl::skip(1) | rpl::on_next([=] {
		refreshColors();
	}, lifetime());
	style::PaletteChanged(
	) | rpl::on_next([=] {
		refreshColors();
	}, lifetime());

	const auto instance = ::Media::Player::instance();
	rpl::merge(
		instance->updatedNotifier() | rpl::to_empty,
		instance->trackChanged() | rpl::to_empty,
		instance->tracksFinished() | rpl::to_empty
	) | rpl::on_next([=] {
		const auto now = playing();
		if (_playing != now) {
			_playing = now;
			update();
		}
	}, lifetime());
}

void Plate::setItem(HistoryItem *item) {
	const auto media = item ? item->media() : nullptr;
	const auto document = media ? media->document() : nullptr;
	_item = document ? item : nullptr;
	if (_document == document) {
		return;
	}
	_document = document;
	_media = document ? document->createMediaView() : nullptr;
	_cover = QImage();
	_coverGood = false;
	_downloadLifetime.destroy();
	if (document) {
		const auto name = Ui::Text::FormatSongNameFor(document);
		const auto composed = name.composedName();
		_title = composed.title;
		_performer = composed.performer.isEmpty()
			? tr::lng_sg_music_unknown_artist(tr::now)
			: composed.performer;
		if (document->hasThumbnail()) {
			document->loadThumbnail(item->fullId());
			document->session().downloaderTaskFinished(
			) | rpl::on_next([=] {
				if (!_coverGood) {
					_cover = QImage();
					update();
				}
			}, _downloadLifetime);
		}
	} else {
		_title = _performer = QString();
	}
	_playing = playing();
	update();
}

bool Plate::playing() const {
	if (!_document) {
		return false;
	}
	using Type = AudioMsgId::Type;
	const auto instance = ::Media::Player::instance();
	const auto state = instance->getState(Type::Song);
	return (state.id.audio() == _document)
		&& ::Media::Player::ShowPauseIcon(state.state);
}

void Plate::clicked() {
	if (!_item || !_document) {
		return;
	}
	// The saved music item makes the playlist go through the user's
	// profile music (SparseIdsMergedSlice::kSavedMusicTopicId).
	::Media::Player::instance()->playPause(
		AudioMsgId(_document, _item->fullId()));
}

auto Plate::computeColors() const -> Colors {
	auto result = Colors();
	if (const auto collectible = _user->emojiStatusId().collectible) {
		result.center = collectible->centerColor;
		result.edge = collectible->edgeColor;
		result.pattern = collectible->patternColor;
		result.patternId = collectible->patternDocumentId;
		return result;
	}
	result.patternId = _user->profileBackgroundEmojiId();
	const auto profile = _user->session().api().peerColors().colorProfileFor(
		_user);
	if (profile && !profile->bg.empty()) {
		// Android: the radial gradient from the right edge.
		result.center = (profile->bg.size() > 1)
			? profile->bg[1]
			: profile->bg.front();
		result.edge = profile->bg.front();
		result.pattern = QColor(0, 0, 0);
	} else {
		result.pattern = st::windowSubTextFg->c;
	}
	return result;
}

void Plate::refreshColors() {
	_colors = computeColors();
	_background = QImage();
	_patternCache.clear();
	refreshPattern(_colors.patternId);
	update();
}

void Plate::refreshPattern(DocumentId id) {
	if (_patternEmojiId == id) {
		return;
	}
	_patternEmojiId = id;
	_patternCache.clear();
	if (!id) {
		_patternEmoji = nullptr;
		return;
	}
	const auto owner = &_user->owner();
	_patternEmoji = owner->customEmojiManager().create(
		owner->document(id),
		[=] { update(); },
		Data::CustomEmojiSizeTag::Large);
}

void Plate::validateBackground() {
	const auto ratio = style::DevicePixelRatio();
	if (!_background.isNull()
		&& _background.size() == size() * ratio) {
		return;
	}
	_background = QImage(
		size() * ratio,
		QImage::Format_ARGB32_Premultiplied);
	_background.setDevicePixelRatio(ratio);
	_background.fill(Qt::transparent);
	auto p = QPainter(&_background);
	auto hq = PainterHighQualityEnabler(p);
	auto path = QPainterPath();
	const auto radius = std::min(Px(kRadius), height() / 2);
	path.addRoundedRect(rect(), radius, radius);
	if (_colors.center && _colors.edge) {
		auto gradient = QRadialGradient(
			QPointF(width(), height() / 2.),
			width() * 1.);
		gradient.setStops({
			{ 0., *_colors.center },
			{ 1., *_colors.edge },
		});
		p.fillPath(path, gradient);
	} else {
		p.fillPath(path, st::windowBgOver);
	}
}

void Plate::paintPattern(QPainter &p) {
	if (!_patternEmoji || !_patternEmoji->ready()) {
		return;
	}
	const auto area = QRect(
		width() - Px(kPatternWidth),
		0,
		Px(kPatternWidth),
		height());
	if (area.width() <= 0 || area.height() <= 0) {
		return;
	}
	if (_patternFor != area.size()) {
		_patternFor = area.size();
		_patternCache.clear();
		_patternPoints.clear();
		const auto large = Ui::Emoji::GetSizeLarge()
			/ float64(style::DevicePixelRatio());
		for (const auto &[x, y, size, opacity] : kPattern) {
			_patternPoints.push_back({
				.position = QPointF(
					(area.width() + Px(x)) / float64(area.width()),
					(area.height() / 2. + Px(y)) / area.height()),
				.scale = std::min(Px(size) / large, 1.),
				.opacity = opacity,
			});
		}
	}
	auto hq = PainterHighQualityEnabler(p);
	Ui::PaintBgPoints(
		p,
		_patternPoints,
		_patternCache,
		_patternEmoji.get(),
		_colors.pattern,
		area);
}

void Plate::validateCover() {
	if (!_media || (!_cover.isNull() && _coverGood)) {
		return;
	}
	const auto good = _media->thumbnail();
	const auto image = good ? good : _media->thumbnailInline();
	if (!image) {
		return;
	}
	if (!_cover.isNull() && !good) {
		return;
	}
	const auto ratio = style::DevicePixelRatio();
	const auto side = Px(kCover) * ratio;
	auto original = image->original();
	if (original.isNull()) {
		return;
	}
	original = original.scaled(
		side,
		side,
		Qt::KeepAspectRatioByExpanding,
		Qt::SmoothTransformation);
	_cover = original.copy(
		(original.width() - side) / 2,
		(original.height() - side) / 2,
		side,
		side);
	_cover.setDevicePixelRatio(ratio);
	_coverGood = (good != nullptr);
	if (_coverGood) {
		_downloadLifetime.destroy();
	}
}

void Plate::paintCover(QPainter &p, QRect rect) {
	auto hq = PainterHighQualityEnabler(p);
	auto path = QPainterPath();
	path.addRoundedRect(rect, Px(kCoverRadius), Px(kCoverRadius));
	validateCover();
	if (!_cover.isNull()) {
		p.save();
		p.setClipPath(path);
		p.drawImage(rect, _cover);
		p.restore();
	} else {
		const auto colored = _colors.center.has_value();
		p.fillPath(path, colored ? QColor(0, 0, 0, 48) : st::windowBg->c);
		st::historyFileThumbPlay.paintInCenter(
			p,
			rect,
			colored ? QColor(255, 255, 255) : st::windowSubTextFg->c);
	}

	// Play / pause badge over the cover.
	if (!_cover.isNull() && (_playing || isOver())) {
		const auto size = Px(kPlayButton);
		const auto badge = QRect(
			rect.x() + (rect.width() - size) / 2,
			rect.y() + (rect.height() - size) / 2,
			size,
			size);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0, 0, 0, 110));
		p.drawEllipse(badge);
		(_playing
			? st::historyFileThumbPause
			: st::historyFileThumbPlay).paintInCenter(p, badge);
	}
}

void Plate::paintTexts(QPainter &p, QRect rect) {
	const auto colored = _colors.center.has_value();
	const auto fg = colored ? QColor(255, 255, 255) : st::windowFg->c;
	auto sub = colored ? QColor(255, 255, 255, 153) : st::windowSubTextFg->c;
	const auto &titleFont = st::semiboldFont;
	const auto &subFont = st::normalFont;
	const auto gap = Px(2);
	const auto total = titleFont->height + gap + subFont->height;
	auto top = rect.y() + (rect.height() - total) / 2;
	p.setPen(fg);
	p.setFont(titleFont);
	p.drawText(
		rect.x(),
		top + titleFont->ascent,
		titleFont->elided(_title, rect.width()));
	top += titleFont->height + gap;
	p.setPen(sub);
	p.setFont(subFont);
	p.drawText(
		rect.x(),
		top + subFont->ascent,
		subFont->elided(_performer, rect.width()));
}

void Plate::onStateChanged(State was, StateChangeSource source) {
	update();
}

int Plate::resizeGetHeight(int newWidth) {
	return Px(kPadding) * 2 + Px(kCover);
}

void Plate::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	validateBackground();
	p.drawImage(0, 0, _background);
	{
		auto path = QPainterPath();
		const auto radius = std::min(Px(kRadius), height() / 2);
		path.addRoundedRect(rect(), radius, radius);
		p.save();
		p.setClipPath(path);
		paintPattern(p);
		if (isDown()) {
			p.fillRect(rect(), QColor(0, 0, 0, 24));
		}
		p.restore();
	}
	const auto padding = Px(kPadding);
	const auto cover = QRect(padding, padding, Px(kCover), Px(kCover));
	paintCover(p, cover);
	const auto textLeft = cover.x() + cover.width() + padding;
	paintTexts(
		p,
		QRect(
			textLeft,
			padding,
			std::max(width() - textLeft - padding, 1),
			Px(kCover)));
}

} // namespace

rpl::producer<bool> ProfileMusicPlateShownValue(not_null<UserData*> user) {
	return FirstSavedMusicValue(user) | rpl::map([](HistoryItem *item) {
		return (item != nullptr);
	});
}

object_ptr<Ui::RpWidget> CreateProfileMusicPlate(
		not_null<Ui::RpWidget*> parent,
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user) {
	auto result = object_ptr<Plate>(parent.get(), controller, user);
	const auto raw = result.data();
	FirstSavedMusicValue(
		user
	) | rpl::on_next([=](HistoryItem *item) {
		raw->setItem(item);
	}, raw->lifetime());
	raw->resizeToWidth(parent->width());
	return result;
}

} // namespace StaticGram
