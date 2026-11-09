/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/unread_badge.h"

#include "data/data_emoji_statuses.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "data/data_session.h"
#include "data/stickers/data_custom_emoji.h"
#include "main/main_session.h"
#include "lang/lang_keys.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/power_saving.h"
#include "ui/text/text_custom_emoji.h"
#include "ui/unread_badge_paint.h"
#include "styles/style_dialogs.h"
#include "staticgram/sg_badges.h"

namespace Ui {
namespace {

constexpr auto kPlayStatusLimit = 2;
constexpr auto kBotVerifiedScale = 0.88;

class ScaledBotVerifiedEmoji final : public Ui::Text::CustomEmoji {
public:
	ScaledBotVerifiedEmoji(
		std::unique_ptr<Ui::Text::CustomEmoji> wrapped,
		int innerSize,
		int outerSize);

	int width() override;
	QString entityData() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

private:
	const std::unique_ptr<Ui::Text::CustomEmoji> _wrapped;
	const int _innerSize = 0;
	const int _outerSize = 0;
	QImage _frame;
	QColor _frameColor;

};

ScaledBotVerifiedEmoji::ScaledBotVerifiedEmoji(
	std::unique_ptr<Ui::Text::CustomEmoji> wrapped,
	int innerSize,
	int outerSize)
: _wrapped(std::move(wrapped))
, _innerSize(innerSize)
, _outerSize(outerSize) {
}

int ScaledBotVerifiedEmoji::width() {
	return _outerSize;
}

QString ScaledBotVerifiedEmoji::entityData() {
	return _wrapped->entityData();
}

void ScaledBotVerifiedEmoji::paint(QPainter &p, const Context &context) {
	if (_frame.isNull() || _frameColor != context.textColor) {
		if (!_wrapped->ready()) {
			return;
		}
		const auto ratio = style::DevicePixelRatio();
		const auto sourcePx = Data::FrameSizeFromTag(
			Data::CustomEmojiSizeTag::Isolated);
		_frame = QImage(
			QSize(sourcePx, sourcePx),
			QImage::Format_ARGB32_Premultiplied);
		_frame.setDevicePixelRatio(ratio);
		_frame.fill(Qt::transparent);

		auto painter = QPainter(&_frame);
		painter.translate(-context.position);
		const auto was = context.internal.forceFirstFrame;
		context.internal.forceFirstFrame = true;
		_wrapped->paint(painter, context);
		context.internal.forceFirstFrame = was;
		painter.end();

		_frame = _frame.scaled(
			QSize(_innerSize, _innerSize) * ratio,
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation);
		_frameColor = context.textColor;
	}
	const auto skip = (_outerSize - _innerSize) / 2;
	p.drawImage(context.position + QPoint(skip, skip), _frame);
}

void ScaledBotVerifiedEmoji::unload() {
	_wrapped->unload();
}

bool ScaledBotVerifiedEmoji::ready() {
	return !_frame.isNull() || _wrapped->ready();
}

bool ScaledBotVerifiedEmoji::readyInDefaultState() {
	return !_frame.isNull() || _wrapped->ready();
}

} // namespace

struct PeerBadge::EmojiStatus {
	EmojiStatusId id;
	std::unique_ptr<Ui::Text::CustomEmoji> emoji;
	QPoint lastPosition;
	QColor lastColor;
	int skip = 0;
	bool painted = false;
};

// StaticGram: exteraStatic badge (a custom emoji) after the emoji status.
struct PeerBadge::StaticGramBadge {
	DocumentId id = 0;
	std::unique_ptr<Ui::Text::CustomEmoji> emoji;
	int skip = 0;
};

struct PeerBadge::BotVerifiedData {
	QImage cache;
	std::unique_ptr<Text::CustomEmoji> icon;
};

void UnreadBadge::setText(const QString &text, bool active) {
	_text = text;
	_active = active;
	const auto st = Dialogs::Ui::UnreadBadgeStyle();
	resize(
		std::max(st.font->width(text) + 2 * st.padding, st.size),
		st.size);
	update();
}

int UnreadBadge::textBaseline() const {
	const auto st = Dialogs::Ui::UnreadBadgeStyle();
	return ((st.size - st.font->height) / 2) + st.font->ascent;
}

void UnreadBadge::paintEvent(QPaintEvent *e) {
	if (_text.isEmpty()) {
		return;
	}

	auto p = QPainter(this);

	UnreadBadgeStyle unreadSt;
	unreadSt.muted = !_active;
	auto unreadRight = width();
	auto unreadTop = 0;
	PaintUnreadBadge(
		p,
		_text,
		unreadRight,
		unreadTop,
		unreadSt);
}

QString TextBadgeText(TextBadgeType type) {
	switch (type) {
	case TextBadgeType::Fake: return tr::lng_fake_badge(tr::now);
	case TextBadgeType::Scam: return tr::lng_scam_badge(tr::now);
	case TextBadgeType::Direct: return tr::lng_direct_badge(tr::now);
	}
	Unexpected("Type in TextBadgeText.");
}

QSize TextBadgeSize(TextBadgeType type) {
	const auto phrase = TextBadgeText(type);
	const auto phraseWidth = st::dialogsScamFont->width(phrase);
	const auto width = st::dialogsScamPadding.left()
		+ phraseWidth
		+ st::dialogsScamPadding.right();
	const auto height = st::dialogsScamPadding.top()
		+ st::dialogsScamFont->height
		+ st::dialogsScamPadding.bottom();
	return { width, height };
}

void DrawTextBadge(
		Painter &p,
		QRect rect,
		int outerWidth,
		const style::color &color,
		const QString &phrase,
		int phraseWidth) {
	PainterHighQualityEnabler hq(p);
	auto pen = color->p;
	pen.setWidth(st::lineWidth);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawRoundedRect(rect, st::dialogsScamRadius, st::dialogsScamRadius);
	p.setFont(st::dialogsScamFont);
	if (style::DevicePixelRatio() > 1) {
		p.drawText(
			QRect(
				rect.x() + st::dialogsScamPadding.left(),
				rect.y() + st::dialogsScamPadding.top(),
				rect.width() - rect::m::sum::h(st::dialogsScamPadding),
				rect.height() - rect::m::sum::v(st::dialogsScamPadding)),
			Qt::AlignCenter,
			phrase);
	} else {
		p.drawTextLeft(
			rect.x() + st::dialogsScamPadding.left(),
			rect.y() + st::dialogsScamPadding.top(),
			outerWidth,
			phrase,
			phraseWidth);
	}
}

void DrawTextBadge(
		TextBadgeType type,
		Painter &p,
		QRect rect,
		int outerWidth,
		const style::color &color) {
	const auto phrase = TextBadgeText(type);
	DrawTextBadge(
		p,
		rect,
		outerWidth,
		color,
		phrase,
		st::dialogsScamFont->width(phrase));
}

PeerBadge::PeerBadge() = default;

PeerBadge::~PeerBadge() = default;

int PeerBadge::drawGetWidth(Painter &p, Descriptor &&descriptor) {
	Expects(descriptor.customEmojiRepaint != nullptr);

	const auto peer = descriptor.peer;
	if ((descriptor.scam && (peer->isScam() || peer->isFake()))
		|| (descriptor.direct && peer->isMonoforum())) {
		if (_emojiStatus) {
			_emojiStatus->painted = false;
		}
		_staticGramBadge = nullptr;
		return drawTextBadge(p, descriptor);
	}
	const auto verifyCheck = descriptor.verified && peer->isVerified();
	const auto premiumMark = descriptor.premium
		&& peer->session().premiumBadgesShown();
	const auto emojiStatus = premiumMark
		&& peer->emojiStatusId()
		&& (peer->isPremium() || peer->isChannel())
		&& !peer->hideEmojiStatus();
	const auto premiumStar = premiumMark
		&& !emojiStatus
		&& peer->isPremium();

	const auto paintVerify = verifyCheck
		&& (descriptor.prioritizeVerification
			|| descriptor.bothVerifyAndStatus
			|| !emojiStatus);
	const auto paintEmoji = emojiStatus
		&& (!paintVerify || descriptor.bothVerifyAndStatus);
	const auto paintStar = premiumStar && !paintVerify;

	// StaticGram: the exteraStatic badge goes between the emoji status and
	// the verified check, like on Android: name [status] [badge] [verified].
	const auto staticGramId = (descriptor.premium
		&& descriptor.premiumFg
		&& !peer->isScam()
		&& !peer->isFake())
		? StaticGram::BadgeDocumentId(peer)
		: DocumentId();
	const auto staticGramWidth = staticGramId
		? staticGramBadgeWidth()
		: 0;
	if (!staticGramId) {
		_staticGramBadge = nullptr;
	}
	const auto verifyWidth = paintVerify ? descriptor.verified->width() : 0;

	auto result = 0;
	const auto paintPart = [&](int reserve, auto &&paint) {
		auto &rect = descriptor.rectForName;
		rect.setWidth(rect.width() - reserve);
		const auto width = paint();
		rect.setWidth(rect.width() + reserve);
		descriptor.nameWidth += width;
		result += width;
	};
	if (paintEmoji) {
		paintPart(staticGramWidth + verifyWidth, [&] {
			return drawPremiumEmojiStatus(p, descriptor);
		});
	} else {
		if (_emojiStatus) {
			_emojiStatus->painted = false;
		}
		if (paintStar) {
			paintPart(staticGramWidth, [&] {
				return drawPremiumStar(p, descriptor);
			});
		}
	}
	if (staticGramId) {
		paintPart(verifyWidth, [&] {
			return drawStaticGramBadge(p, descriptor, staticGramId);
		});
	}
	if (paintVerify) {
		result += drawVerifyCheck(p, descriptor);
	}
	return result;
}

int PeerBadge::staticGramBadgeWidth() const {
	const auto size = st::emojiSize;
	const auto skip = (size - Ui::Text::AdjustCustomEmojiSize(size)) / 2;
	return size + st::dialogsChatTypeSkip - 2 * skip;
}

int PeerBadge::drawStaticGramBadge(
		Painter &p,
		const Descriptor &descriptor,
		DocumentId id) {
	if (!_staticGramBadge) {
		_staticGramBadge = std::make_unique<StaticGramBadge>();
		const auto size = st::emojiSize;
		const auto emoji = Ui::Text::AdjustCustomEmojiSize(size);
		_staticGramBadge->skip = (size - emoji) / 2;
	}
	if (_staticGramBadge->id != id) {
		using namespace Ui::Text;
		auto &manager = descriptor.peer->owner().customEmojiManager();
		_staticGramBadge->id = id;
		_staticGramBadge->emoji = MakeWrappedEmoji<LimitedLoopsEmoji>(
			manager.create(id, descriptor.customEmojiRepaint),
			kPlayStatusLimit);
	}
	if (!_staticGramBadge->emoji) {
		return 0;
	}
	const auto width = staticGramBadgeWidth();
	const auto rect = descriptor.rectForName;
	const auto left = rect.x()
		+ std::min(descriptor.nameWidth, rect.width() - width)
		+ st::dialogsChatTypeSkip / 2;
	const auto skip = _staticGramBadge->skip;
	_staticGramBadge->emoji->paint(p, {
		.textColor = (*descriptor.premiumFg)->c,
		.now = descriptor.now,
		.position = QPoint(left - skip, rect.y() + skip),
		.paused = descriptor.paused || On(PowerSaving::kEmojiStatus),
	});
	return width;
}

int PeerBadge::drawTextBadge(Painter &p, const Descriptor &descriptor) {
	const auto type = [&] {
		if (descriptor.peer->isScam()) {
			return TextBadgeType::Scam;
		} else if (descriptor.peer->isFake()) {
			return TextBadgeType::Fake;
		}
		return TextBadgeType::Direct;
	}();
	const auto phrase = TextBadgeText(type);
	const auto phraseWidth = st::dialogsScamFont->width(phrase);
	const auto width = st::dialogsScamPadding.left()
		+ phraseWidth
		+ st::dialogsScamPadding.right();
	const auto height = st::dialogsScamPadding.top()
		+ st::dialogsScamFont->height
		+ st::dialogsScamPadding.bottom();
	const auto rectForName = descriptor.rectForName;
	const auto rect = QRect(
		(rectForName.x()
			+ std::min(
				descriptor.nameWidth + st::dialogsScamSkip,
				rectForName.width() - width)),
		rectForName.y() + (rectForName.height() - height) / 2,
		width,
		height);
	DrawTextBadge(
		p,
		rect,
		descriptor.outerWidth,
		*((type == TextBadgeType::Direct)
			? descriptor.direct
			: descriptor.scam),
		phrase,
		phraseWidth);
	return st::dialogsScamSkip + width;
}

int PeerBadge::drawVerifyCheck(Painter &p, const Descriptor &descriptor) {
	const auto iconw = descriptor.verified->width();
	const auto rectForName = descriptor.rectForName;
	const auto nameWidth = descriptor.nameWidth;
	descriptor.verified->paint(
		p,
		rectForName.x() + std::min(nameWidth, rectForName.width() - iconw),
		rectForName.y(),
		descriptor.outerWidth);
	return iconw;
}

int PeerBadge::drawPremiumEmojiStatus(
		Painter &p,
		const Descriptor &descriptor) {
	const auto peer = descriptor.peer;
	const auto id = peer->emojiStatusId();
	const auto rectForName = descriptor.rectForName;
	const auto iconw = descriptor.premium->width();
	const auto iconx = rectForName.x()
		+ std::min(descriptor.nameWidth, rectForName.width() - iconw);
	const auto icony = rectForName.y();
	if (!_emojiStatus) {
		_emojiStatus = std::make_unique<EmojiStatus>();
		const auto size = st::emojiSize;
		const auto emoji = Ui::Text::AdjustCustomEmojiSize(size);
		_emojiStatus->skip = (size - emoji) / 2;
	}
	if (_emojiStatus->id != id) {
		using namespace Ui::Text;
		auto &manager = peer->session().data().customEmojiManager();
		_emojiStatus->id = id;
		_emojiStatus->emoji = MakeWrappedEmoji<LimitedLoopsEmoji>(
			manager.create(
				Data::EmojiStatusCustomId(id),
				descriptor.customEmojiRepaint),
			kPlayStatusLimit);
	}
	if (!_emojiStatus->emoji) {
		return 0;
	}
	_emojiStatus->lastPosition = QPoint(
		iconx - 2 * _emojiStatus->skip,
		icony + _emojiStatus->skip);
	_emojiStatus->lastColor = (*descriptor.premiumFg)->c;
	_emojiStatus->painted = true;
	_emojiStatus->emoji->paint(p, {
		.textColor = _emojiStatus->lastColor,
		.now = descriptor.now,
		.position = _emojiStatus->lastPosition,
		.paused = descriptor.paused || On(PowerSaving::kEmojiStatus),
	});
	return iconw - 4 * _emojiStatus->skip;
}

int PeerBadge::drawPremiumStar(Painter &p, const Descriptor &descriptor) {
	const auto rectForName = descriptor.rectForName;
	const auto iconw = descriptor.premium->width();
	const auto iconx = rectForName.x()
		+ std::min(descriptor.nameWidth, rectForName.width() - iconw);
	const auto icony = rectForName.y();
	_emojiStatus = nullptr;
	descriptor.premium->paint(p, iconx, icony, descriptor.outerWidth);
	return iconw;
}

QRect PeerBadge::emojiStatusRect() const {
	if (!_emojiStatus || !_emojiStatus->emoji || !_emojiStatus->painted) {
		return QRect();
	}
	return QRect(
		_emojiStatus->lastPosition,
		Size(st::emojiSize - 2 * _emojiStatus->skip));
}

void PeerBadge::paintEmojiStatusFrame(
		QPainter &p,
		crl::time now,
		bool paused) {
	if (!_emojiStatus || !_emojiStatus->emoji || !_emojiStatus->painted) {
		return;
	}
	paintEmojiStatusFrame(p, now, paused, _emojiStatus->lastPosition);
}

void PeerBadge::paintEmojiStatusFrame(
		QPainter &p,
		crl::time now,
		bool paused,
		QPoint position) {
	if (!_emojiStatus || !_emojiStatus->emoji || !_emojiStatus->painted) {
		return;
	}
	_emojiStatus->emoji->paint(p, {
		.textColor = _emojiStatus->lastColor,
		.now = now,
		.position = position,
		.paused = paused || On(PowerSaving::kEmojiStatus),
	});
}

void PeerBadge::unload() {
	_emojiStatus = nullptr;
}

bool PeerBadge::ready(const BotVerifyDetails *details) const {
	if (!details || !*details) {
		_botVerifiedData = nullptr;
		return true;
	} else if (!_botVerifiedData) {
		return false;
	}
	if (!details->iconId) {
		_botVerifiedData->icon = nullptr;
	} else if (!_botVerifiedData->icon
		|| (_botVerifiedData->icon->entityData()
			!= Data::SerializeCustomEmojiId(details->iconId))) {
		return false;
	}
	return true;
}

void PeerBadge::set(
		not_null<const BotVerifyDetails*> details,
		Ui::Text::CustomEmojiFactory factory,
		Fn<void()> repaint) {
	if (!_botVerifiedData) {
		_botVerifiedData = std::make_unique<BotVerifiedData>();
	}
	if (details->iconId) {
		const auto outer = st::emojiSize;
		// Значок mokaGram (botId = 0) такого же размера и на той же линии, что эмодзи-статус.
		const auto inner = details->botId
			? int(base::SafeRound(st::emojiSize * kBotVerifiedScale))
			: int(base::SafeRound(Ui::Text::AdjustCustomEmojiSize(st::emojiSize) * 0.8));
		_botVerifiedData->icon = MakeWrappedEmoji<ScaledBotVerifiedEmoji>(
			factory(
				Data::SerializeCustomEmojiId(details->iconId),
				{ .repaint = repaint }),
			inner,
			outer);
	}
}

int PeerBadge::drawVerified(
		QPainter &p,
		QPoint position,
		const style::VerifiedBadge &st) {
	const auto data = _botVerifiedData.get();
	if (!data) {
		return 0;
	}
	if (const auto icon = data->icon.get()) {
		icon->paint(p, {
			.textColor = st.color->c,
			.now = crl::now(),
			.position = position + st.position,
		});
		return icon->width();
	}
	return 0;
}

} // namespace Ui
