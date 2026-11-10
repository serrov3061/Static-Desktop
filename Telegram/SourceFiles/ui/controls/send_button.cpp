/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/controls/send_button.h"

#include "lang/lang_tag.h"
#include "lottie/lottie_icon.h"
#include "ui/effects/ripple_animation.h"
#include "ui/text/text_utilities.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include <QtSvg/QSvgRenderer>
#include "styles/style_chat_helpers.h"
#include "styles/style_credits.h"
#include "lang/lang_keys.h"
#include "ui/text/format_values.h"

namespace Ui {
namespace {

constexpr auto kWideScale = 5;
constexpr auto kSentOutDuration = crl::time(300);
constexpr auto kSentInDuration = crl::time(380);

// StaticGram: our gem logo (Android send_plane_24 of StaticGram) instead
// of the paper plane, white; tinted when painted.
constexpr auto kGemPath =
		"M 227.988,2.594 c -13.100,3.000 -27.200,9.700 -37.900,17.800 -6.500,5.000 -30.300,24.700 "
		"-32.100,26.600 -0.300,0.300 -6.100,5.200 -13.000,10.900 -6.900,5.800 -17.700,14.800 "
		"-24.000,20.100 -6.300,5.300 -23.900,19.700 -39.000,32.000 -15.100,12.300 -30.200,24.500 "
		"-33.500,27.200 -3.300,2.700 -11.700,9.400 -18.600,14.800 -13.300,10.400 -19.300,16.900 "
		"-23.900,26.000 -4.400,8.700 -6.000,15.500 -6.000,25.400 0.000,10.300 1.500,16.900 "
		"5.800,25.900 3.400,7.100 3.600,7.300 19.600,27.300 6.600,8.200 19.900,24.800 29.600,36.900 "
		"23.500,29.400 54.200,67.400 65.900,81.500 5.200,6.300 12.100,14.600 15.300,18.500 "
		"35.400,43.000 106.400,127.400 156.800,186.500 10.600,12.400 25.100,29.400 32.200,37.800 "
		"14.800,17.400 22.400,24.000 31.800,27.600 14.000,5.400 23.200,5.900 36.500,2.100 "
		"10.600,-3.100 14.200,-4.900 22.200,-11.200 5.900,-4.500 62.700,-51.000 84.700,-69.100 "
		"5.600,-4.600 12.400,-10.300 15.200,-12.500 7.100,-5.800 41.800,-35.000 42.400,-35.600 "
		"0.300,-0.300 3.400,-3.100 7.000,-6.100 3.600,-3.000 6.700,-5.700 7.000,-6.000 0.300,-0.300 "
		"7.000,-6.100 15.000,-13.000 23.300,-20.000 72.900,-64.800 99.000,-89.400 31.200,-29.400 "
		"41.600,-39.200 56.800,-53.600 9.500,-9.100 23.300,-22.100 30.500,-28.900 14.500,-13.500 "
		"19.300,-19.300 24.200,-29.100 4.800,-9.500 6.700,-17.300 6.700,-26.500 0.000,-14.600 "
		"-2.500,-20.800 -17.000,-42.800 -5.900,-9.000 -13.200,-20.200 -16.200,-25.000 -3.000,-4.900 "
		"-7.300,-11.600 -9.500,-15.000 -2.200,-3.400 -9.600,-15.000 -16.400,-25.700 -6.800,-10.700 "
		"-15.000,-23.600 -18.100,-28.500 -3.200,-5.000 -9.400,-14.900 -13.800,-22.100 "
		"-11.400,-18.400 -19.300,-26.100 -32.900,-31.800 -9.700,-4.100 -10.200,-4.200 "
		"-51.300,-11.600 -18.400,-3.400 -41.800,-7.700 -52.000,-9.600 -10.200,-1.900 -22.300,-4.200 "
		"-27.000,-5.000 -15.400,-2.700 -41.400,-7.400 -46.500,-8.400 -18.400,-3.600 -98.500,-17.700 "
		"-159.500,-28.100 -16.200,-2.700 -33.300,-5.700 -38.000,-6.500 -4.700,-0.800 -11.200,-1.900 "
		"-14.500,-2.400 -3.300,-0.500 -10.000,-1.600 -15.000,-2.400 -14.200,-2.500 -35.600,-2.000 "
		"-48.500,1.000 z m 394.000,115.800 c 7.700,3.800 11.800,12.400 10.400,22.000 -1.500,11.600 "
		"-21.000,62.900 -58.100,153.600 -8.000,19.500 -18.500,45.200 -23.400,57.000 -7.600,18.800 "
		"-16.300,39.700 -31.100,75.000 -2.200,5.200 -5.100,12.200 -6.500,15.500 -1.400,3.300 "
		"-3.300,7.800 -4.300,10.000 -0.900,2.200 -3.700,8.500 -6.000,14.000 -13.200,31.300 "
		"-18.800,38.000 -32.700,39.200 -11.700,0.900 -17.100,-2.000 -42.800,-23.200 -14.900,-12.300 "
		"-44.900,-37.300 -57.100,-47.500 -5.500,-4.700 -16.500,-14.000 -24.500,-20.600 "
		"-12.000,-10.100 -14.900,-13.100 -17.300,-17.700 -5.000,-9.900 -3.300,-21.300 4.500,-30.100 "
		"3.300,-3.800 6.100,-6.100 41.400,-34.100 32.700,-25.900 37.400,-29.600 51.900,-41.100 "
		"59.200,-46.500 83.300,-66.500 102.100,-84.600 9.700,-9.300 11.600,-11.600 11.300,-13.700 "
		"-0.700,-6.300 -11.100,-2.900 -70.300,22.900 -73.400,32.100 -173.300,74.500 -196.200,83.300 "
		"-12.700,4.900 -18.700,6.200 -28.800,6.100 -10.700,-0.100 -17.100,-1.700 -34.300,-8.600 "
		"-17.300,-6.900 -44.600,-18.500 -50.500,-21.500 -11.200,-5.600 -17.600,-14.900 "
		"-16.300,-23.500 1.400,-8.700 7.800,-14.200 22.600,-19.500 11.900,-4.300 53.200,-17.100 "
		"72.000,-22.300 3.600,-1.000 11.900,-3.400 18.500,-5.300 15.400,-4.500 58.200,-16.200 "
		"70.000,-19.200 4.900,-1.300 14.200,-3.800 20.500,-5.500 6.300,-1.700 16.500,-4.500 "
		"22.500,-6.000 6.100,-1.600 16.900,-4.500 24.000,-6.500 14.700,-4.000 32.100,-8.600 "
		"57.000,-15.100 9.400,-2.400 24.900,-6.400 34.500,-9.000 9.600,-2.500 21.100,-5.400 "
		"25.500,-6.500 4.400,-1.000 11.400,-2.800 15.500,-3.900 19.400,-5.000 61.000,-14.000 "
		"75.500,-16.300 5.700,-0.900 15.900,0.500 20.500,2.700 z";

[[nodiscard]] QImage GenerateGem(QSize size, QColor color) {
	const auto ratio = style::DevicePixelRatio();
	auto result = QImage(size * ratio, QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(ratio);
	result.fill(Qt::transparent);
	const auto svg = QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\" "
		"width=\"822\" height=\"822\" viewBox=\"89 89 822 822\">"
		"<g transform=\"translate(89.96,168.5) scale(1.02)\">"
		"<path fill=\"#ffffff\" fill-rule=\"evenodd\" d=\"")
		+ QByteArray(kGemPath)
		+ QByteArray("\"/></g></svg>");
	auto renderer = QSvgRenderer(svg);
	{
		auto p = QPainter(&result);
		auto hq = PainterHighQualityEnabler(p);
		renderer.render(&p, QRectF(QPointF(), QSizeF(size)));
		p.setCompositionMode(QPainter::CompositionMode_SourceIn);
		p.fillRect(QRect(QPoint(), size), color);
	}
	return result;
}

[[nodiscard]] float64 EaseOutBack(float64 t) {
	constexpr auto c1 = 1.70158;
	constexpr auto c3 = c1 + 1.;
	const auto x = t - 1.;
	return 1. + c3 * x * x * x + c1 * x * x;
}
constexpr auto kVoiceToRoundIndex = 0;
constexpr auto kRoundToVoiceIndex = 1;
constexpr auto kForbiddenOpacity = 0.5;

} // namespace

SendButton::SendButton(QWidget *parent, const style::SendButton &st)
: RippleButton(parent, st.inner.ripple)
, _st(st)
, _lastRippleShape(currentRippleShape()) {
	updateSize();
}

SendButton::~SendButton() = default;

void SendButton::setState(State state) {
	if (_state == state) {
		return;
	}

	const auto previousType = _state.type;
	const auto newType = state.type;
	const auto voiceRoundTransition = isVoiceRoundTransition(
		previousType,
		newType);

	const auto hasSlowmode = (_state.slowmodeDelay > 0);
	const auto hasSlowmodeChanged = hasSlowmode != (state.slowmodeDelay > 0);
	auto withSameSlowmode = state;
	withSameSlowmode.slowmodeDelay = _state.slowmodeDelay;
	const auto animate = hasSlowmodeChanged
		|| (!hasSlowmode && withSameSlowmode != _state);

	if (animate && !voiceRoundTransition) {
		_contentFrom = grabContent();
	}

	if (_voiceRoundAnimating && !voiceRoundTransition) {
		_voiceRoundAnimating = false;
	}

	if (_state.slowmodeDelay != state.slowmodeDelay) {
		const auto seconds = state.slowmodeDelay;
		const auto minutes = seconds / 60;
		_slowmodeDelayText = seconds
			? u"%1:%2"_q.arg(minutes).arg(seconds % 60, 2, 10, QChar('0'))
			: QString();
	}
	if (!state.starsToSend || state.type != Type::Send) {
		_starsToSendText = Text::String();
	} else if (_starsToSendText.isEmpty()
		|| _state.starsToSend != state.starsToSend) {
		_starsToSendText.setMarkedText(
			_st.stars.style,
			Text::IconEmoji(&st::starIconEmoji).append(
				Lang::FormatCountToShort(state.starsToSend).string),
			kMarkupTextOptions);
	}
	_state = state;

	const auto newShape = currentRippleShape();
	if (_lastRippleShape != newShape) {
		_lastRippleShape = newShape;
		RippleButton::finishAnimating();
	}

	setAccessibleName([&] {
		switch (_state.type) {
		case Type::Send: return tr::lng_send_button(tr::now);
		case Type::Stop: return tr::lng_stop_button(tr::now);
		case Type::Record:
			return tr::lng_shortcuts_record_voice_message(tr::now);
		case Type::Round:
			return tr::lng_shortcuts_record_round_message(tr::now);
		case Type::Cancel: return tr::lng_cancel(tr::now);
		case Type::Save: return tr::lng_settings_save(tr::now);
		case Type::Slowmode:
			return tr::lng_slowmode_enabled(
				tr::now,
				lt_left,
				Ui::FormatDurationWordsSlowmode(_state.slowmodeDelay));
		case Type::Schedule: return tr::lng_schedule_button(tr::now);
		case Type::EditPrice:
			return tr::lng_suggest_menu_edit_price(tr::now);
		}
		Unexpected("Send button type.");
	}());

	if (voiceRoundTransition) {
		_voiceRoundAnimating = true;

		const auto toRound = (newType == Type::Round);
		const auto index = toRound ? kVoiceToRoundIndex : kRoundToVoiceIndex;
		auto &icon = _voiceRoundIcons[index];
		if (!icon) {
			initVoiceRoundIcon(index);
		}
		icon->animate([=, raw = icon.get()] {
			update();
			if (!raw->animating()) {
				_voiceRoundAnimating = false;
			}
		}, 0, icon->framesCount() - 1);
		auto &after = _voiceRoundIcons[1 - index];
		if (!after) {
			initVoiceRoundIcon(1 - index);
		} else if (after->frameIndex() != 0) {
			after->jumpTo(0, nullptr);
		}
	} else if (animate) {
		_stateChangeFromWidth = width();
		_stateChangeAnimation.stop();
		updateSize();
		_contentTo = grabContent();
		_stateChangeAnimation.start(
			[=] { updateSize(); update(); },
			0.,
			1.,
			st::universalDuration);
		setPointerCursor(_state.type != Type::Slowmode);
		updateSize();
	}
	update();
}

void SendButton::finishAnimating() {
	_stateChangeAnimation.stop();
	updateSize();
	update();
}

void SendButton::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);

	auto over = (isDown() || isOver());

	if (_voiceRoundAnimating) {
		paintVoiceRoundIcon(p, over);
		return;
	}

	if (const auto sent = _sentAnimation.value(1.); sent < 1.) {
		paintSentAnimation(p, over, sent);
		return;
	}

	auto changed = _stateChangeAnimation.value(1.);
	if (changed < 1.) {
		PainterHighQualityEnabler hq(p);
		const auto ratio = style::DevicePixelRatio();

		p.setOpacity(1. - changed);
		const auto fromSize = _contentFrom.size() / (kWideScale * ratio);
		const auto fromShift = QPoint(
			(width() - fromSize.width()) / 2,
			(height() - fromSize.height()) / 2);
		auto fromRect = QRect(
			(1 - kWideScale) / 2 * fromSize.width(),
			(1 - kWideScale) / 2 * fromSize.height(),
			kWideScale * fromSize.width(),
			kWideScale * fromSize.height()
		).translated(fromShift);
		auto hiddenWidth = anim::interpolate(0, (1 - kWideScale) / 2 * fromSize.width(), changed);
		auto hiddenHeight = anim::interpolate(0, (1 - kWideScale) / 2 * fromSize.height(), changed);
		p.drawPixmap(
			fromRect.marginsAdded(
				{ hiddenWidth, hiddenHeight, hiddenWidth, hiddenHeight }),
			_contentFrom);

		p.setOpacity(changed);
		const auto toSize = _contentTo.size() / (kWideScale * ratio);
		const auto toShift = QPoint(
			(width() - toSize.width()) / 2,
			(height() - toSize.height()) / 2);
		auto toRect = QRect(
			(1 - kWideScale) / 2 * toSize.width(),
			(1 - kWideScale) / 2 * toSize.height(),
			kWideScale * toSize.width(),
			kWideScale * toSize.height()
		).translated(toShift);
		auto shownWidth = anim::interpolate((1 - kWideScale) / 2 * width(), 0, changed);
		auto shownHeight = anim::interpolate((1 - kWideScale) / 2 * toSize.height(), 0, changed);
		p.drawPixmap(
			toRect.marginsAdded(
				{ shownWidth, shownHeight, shownWidth, shownHeight }),
			_contentTo);
		return;
	}
	paintContent(p, over);
}

void SendButton::paintContent(QPainter &p, bool over) {
	switch (_state.type) {
	case Type::Record: paintRecord(p, over); break;
	case Type::Round: paintRound(p, over); break;
	case Type::Save: paintSave(p, over); break;
	case Type::Cancel: paintCancel(p, over); break;
	case Type::Send:
		if (_starsToSendText.isEmpty()) {
			paintSend(p, over);
		} else {
			paintStarsToSend(p, over);
		}
		break;
	case Type::Stop: paintStop(p, over); break;
	case Type::Schedule: paintSchedule(p, over); break;
	case Type::Slowmode: paintSlowmode(p); break;
	case Type::EditPrice: break;
	}
}

void SendButton::playSentAnimation() {
	_sentAnimation.stop();
	_sentAnimation.start(
		[=] { update(); },
		0.,
		1.,
		kSentOutDuration + kSentInDuration,
		anim::linear);
}

void SendButton::paintGem(QPainter &p, QRect rect, QColor color) {
	const auto ratio = style::DevicePixelRatio();
	if (_gem.isNull()
		|| _gemColor != color
		|| _gem.size() != rect.size() * ratio) {
		_gem = GenerateGem(rect.size(), color);
		_gemColor = color;
	}
	p.drawImage(rect, _gem);
}

void SendButton::paintSentAnimation(
		QPainter &p,
		bool over,
		float64 progress) {
	const auto time = progress * (kSentOutDuration + kSentInDuration);
	if (time >= kSentOutDuration) {
		// The current content springs back in from below.
		const auto t = std::min((time - kSentOutDuration) / kSentInDuration, 1.);
		const auto spring = EaseOutBack(t);
		p.save();
		p.setOpacity(p.opacity() * std::min(1., t * 2.));
		p.translate(0., (1. - spring) * height() * 0.5);
		paintContent(p, over);
		p.restore();
		return;
	}
	// The sent gem squashes with a tilt, then jumps up spinning and fades.
	const auto q = time / kSentOutDuration;
	auto scale = 1.;
	auto angle = 0.;
	auto shift = 0.;
	auto opacity = 1.;
	if (q < 0.4) {
		const auto s = 1. - std::pow(1. - q / 0.4, 3);
		scale = 1. - 0.22 * s;
		angle = -10. * s;
	} else {
		const auto s = (q - 0.4) / 0.6;
		const auto e = s * s;
		scale = 0.78 + 0.34 * e;
		angle = -10. + 34. * e;
		shift = -e * height();
		opacity = 1. - s;
	}
	const auto rect = style::rtlrect(
		QRect(_st.sendIconPosition, _st.inner.icon.size()),
		width());
	auto hq = PainterHighQualityEnabler(p);
	p.save();
	p.setOpacity(p.opacity() * opacity);
	p.translate(QPointF(rect.center()) + QPointF(0., shift));
	p.rotate(angle);
	p.scale(scale, scale);
	paintGem(
		p,
		QRect(
			-rect.width() / 2,
			-rect.height() / 2,
			rect.width(),
			rect.height()),
		_st.sendIconFg->c);
	p.restore();
}

void SendButton::paintRecord(QPainter &p, bool over) {
	if (!isDisabled() && !_state.forbidden) {
		paintRipple(
			p,
			(width() - _st.inner.rippleAreaSize) / 2,
			_st.inner.rippleAreaPosition.y());
	}
	if (_state.forbidden) {
		p.setOpacity(kForbiddenOpacity);
	}
	paintLottieIcon(p, kVoiceToRoundIndex, over);
	if (_state.forbidden) {
		p.setOpacity(1.);
	}
}

void SendButton::paintRound(QPainter &p, bool over) {
	if (!isDisabled() && !_state.forbidden) {
		paintRipple(
			p,
			(width() - _st.inner.rippleAreaSize) / 2,
			_st.inner.rippleAreaPosition.y());
	}
	if (_state.forbidden) {
		p.setOpacity(kForbiddenOpacity);
	}
	paintLottieIcon(p, kRoundToVoiceIndex, over);
	if (_state.forbidden) {
		p.setOpacity(1.);
	}
}

void SendButton::paintLottieIcon(QPainter &p, int index, bool over) {
	auto &icon = _voiceRoundIcons[index];
	if (!icon) {
		initVoiceRoundIcon(index);
	} else if (!_voiceRoundAnimating && icon->frameIndex() != 0) {
		icon->jumpTo(0, [=] { update(); });
	}
	const auto color = (isDisabled() || !over)
		? st::historyRecordVoiceFg->c
		: st::historyRecordVoiceFgOver->c;
	icon->paintInCenter(p, rect(), color);
}

void SendButton::paintSave(QPainter &p, bool over) {
	if (!isDisabled()) {
		auto color = _st.sendIconFg->c;
		color.setAlpha(25);
		paintRipple(
			p,
			(width() - _st.inner.rippleAreaSize) / 2,
			_st.inner.rippleAreaPosition.y(),
			&color);
	}
	const auto &saveIcon = over
		? st::historyEditSaveIconOver
		: st::historyEditSaveIcon;
	saveIcon.paintInCenter(p, rect());
}

void SendButton::paintCancel(QPainter &p, bool over) {
	paintRipple(
		p,
		(width() - _st.inner.rippleAreaSize) / 2,
		_st.inner.rippleAreaPosition.y());

	const auto &cancelIcon = over
		? st::historyReplyCancelIconOver
		: st::historyReplyCancelIcon;
	cancelIcon.paintInCenter(p, rect());
}

void SendButton::paintSend(QPainter &p, bool over) {
	const auto &sendIcon = over ? _st.inner.iconOver : _st.inner.icon;
	if (const auto padding = _st.sendIconFillPadding; padding > 0) {
		const auto ellipse = sendEllipseRect();
		{
			auto hq = PainterHighQualityEnabler(p);
			p.setPen(Qt::NoPen);
			if (_state.fillBgOverride.isValid()) {
				p.setBrush(_state.fillBgOverride);
			} else {
				p.setBrush(st::windowBgActive);
			}
			p.drawEllipse(ellipse);
		}
		if (!isDisabled()) {
			auto color = _st.sendIconFg->c;
			color.setAlpha(25);
			paintRipple(p, ellipse.topLeft(), &color);
		}
	} else if (!isDisabled()) {
		auto color = _st.sendIconFg->c;
		color.setAlpha(25);
		paintRipple(
			p,
			(width() - _st.inner.rippleAreaSize) / 2,
			_st.inner.rippleAreaPosition.y(),
			&color);
	}
	// StaticGram: the gem logo instead of the paper plane.
	const auto rect = style::rtlrect(
		QRect(_st.sendIconPosition, sendIcon.size()),
		width());
	paintGem(
		p,
		rect,
		(isDisabled()
			? st::historyRecordVoiceFg->c
			: (over && _st.sendIconFillPadding <= 0)
			? st::historySendIconFgOver->c
			: _st.sendIconFg->c));
}

void SendButton::paintStop(QPainter &p, bool over) {
	if (!isDisabled()) {
		paintRipple(
			p,
			(width() - _st.inner.rippleAreaSize) / 2,
			_st.inner.rippleAreaPosition.y());
	}
	auto hq = PainterHighQualityEnabler(p);
	const auto size = _st.stopSize;
	const auto inner = QRect(
		QPoint(
			(width() - size.width()) / 2,
			(height() - size.height()) / 2),
		size);
	p.setPen(Qt::NoPen);
	p.setBrush(over ? st::historySendIconFgOver : st::historySendIconFg);
	p.drawRoundedRect(inner, _st.stopRadius, _st.stopRadius);
}

void SendButton::paintStarsToSend(QPainter &p, bool over) {
	const auto geometry = starsGeometry();
	{
		PainterHighQualityEnabler hq(p);
		p.setPen(Qt::NoPen);
		if (_state.fillBgOverride.isValid()) {
			p.setBrush(_state.fillBgOverride);
		} else {
			p.setBrush(over ? _st.stars.textBgOver : _st.stars.textBg);
		}
		const auto radius = geometry.rounded.height() / 2;
		p.drawRoundedRect(geometry.rounded, radius, radius);
	}
	if (!isDisabled()) {
		auto color = _st.stars.textFg->c;
		color.setAlpha(25);
		paintRipple(p, geometry.rounded.topLeft(), &color);
	}
	p.setPen(over ? _st.stars.textFgOver : _st.stars.textFg);
	_starsToSendText.draw(p, {
		.position = geometry.inner.topLeft(),
		.outerWidth = width(),
		.availableWidth = geometry.inner.width(),
	});
}

void SendButton::paintSchedule(QPainter &p, bool over) {
	const auto ellipse = scheduleEllipseRect();
	{
		PainterHighQualityEnabler hq(p);
		p.setPen(Qt::NoPen);
		p.setBrush(over ? st::historySendIconFgOver : st::historySendIconFg);
		p.drawEllipse(ellipse);
	}
	if (!isDisabled()) {
		auto color = st::historyComposeAreaBg->c;
		color.setAlpha(25);
		paintRipple(p, ellipse.topLeft(), &color);
	}
	st::historyScheduleIcon.paint(
		p,
		st::historyScheduleIconPosition,
		width());
}

void SendButton::paintSlowmode(QPainter &p) {
	p.setFont(st::normalFont);
	p.setPen(st::windowSubTextFg);
	p.drawText(
		rect().marginsRemoved(st::historySlowmodeCounterMargins),
		_slowmodeDelayText,
		style::al_center);
}

SendButton::StarsGeometry SendButton::starsGeometry() const {
	const auto &st = _st.stars;
	const auto inner = QRect(
		0,
		0,
		_starsToSendText.maxWidth(),
		st.style.font->height);
	const auto rounded = inner.marginsAdded(QMargins(
		st.padding.left() - st.width / 2,
		st.padding.top() + st.textTop,
		st.padding.right() - st.width / 2,
		st.height - st.padding.top() - st.textTop - st.style.font->height));
	const auto add = (_st.inner.height - rounded.height()) / 2;
	const auto outer = rounded.marginsAdded(QMargins(
		add,
		add,
		add,
		_st.inner.height - add - rounded.height()));
	const auto shift = -outer.topLeft();
	return {
		.inner = inner.translated(shift),
		.rounded = rounded.translated(shift),
		.outer = outer.translated(shift),
	};
}

SendButton::RippleShape SendButton::currentRippleShape() const {
	switch (_state.type) {
	case Type::Send:
		if (!_starsToSendText.isEmpty()) {
			return RippleShape::StarsRoundRect;
		} else if (_st.sendIconFillPadding > 0) {
			return RippleShape::SendEllipse;
		}
		return RippleShape::InnerEllipse;
	case Type::Schedule:
		return RippleShape::ScheduleEllipse;
	case Type::Save:
	case Type::Record:
	case Type::Round:
	case Type::Cancel:
	case Type::Stop:
	case Type::Slowmode:
	case Type::EditPrice:
		return RippleShape::InnerEllipse;
	}
	Unexpected("Type in SendButton::currentRippleShape.");
}

QRect SendButton::sendEllipseRect() const {
	const auto &sendIcon = _st.inner.icon;
	const auto padding = _st.sendIconFillPadding;
	const auto natural = QRect(
		_st.sendIconPosition,
		sendIcon.size()
	).marginsAdded({ padding, padding, padding, padding });

	const auto side = std::min({
		natural.width(),
		natural.height(),
		width(),
		height(),
	});
	return QRect(
		(width() - side) / 2,
		(height() - side) / 2,
		side,
		side);
}

QRect SendButton::scheduleEllipseRect() const {
	return QRect(
		st::historyScheduleIconPosition,
		QSize(
			st::historyScheduleIcon.width(),
			st::historyScheduleIcon.height()));
}

void SendButton::updateSize() {
	if (_state.type == Type::EditPrice) {
		resize(0, _st.inner.height);
		return;
	}
	const auto finalWidth = _starsToSendText.isEmpty()
		? _st.inner.width
		: starsGeometry().outer.width();
	const auto progress = _stateChangeAnimation.value(1.);
	resize(
		anim::interpolate(_stateChangeFromWidth, finalWidth, progress),
		_st.inner.height);
}

QPixmap SendButton::grabContent() {
	auto result = QImage(
		kWideScale * size() * style::DevicePixelRatio(),
		QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(style::DevicePixelRatio());
	result.fill(Qt::transparent);
	{
		auto p = QPainter(&result);
		p.drawPixmap(
			(kWideScale - 1) / 2 * width(),
			(kWideScale - 1) / 2 * height(),
			GrabWidget(this));
	}
	return PixmapFromImage(std::move(result));
}

QImage SendButton::prepareRippleMask() const {
	switch (_lastRippleShape) {
	case RippleShape::InnerEllipse: {
		const auto size = _st.inner.rippleAreaSize;
		return RippleAnimation::EllipseMask(QSize(size, size));
	}
	case RippleShape::SendEllipse: {
		const auto r = sendEllipseRect();
		return RippleAnimation::EllipseMask(r.size());
	}
	case RippleShape::StarsRoundRect: {
		const auto r = starsGeometry().rounded;
		const auto radius = r.height() / 2;
		return RippleAnimation::RoundRectMask(r.size(), radius);
	}
	case RippleShape::ScheduleEllipse: {
		const auto r = scheduleEllipseRect();
		return RippleAnimation::EllipseMask(r.size());
	}
	}
	Unexpected("RippleShape in SendButton::prepareRippleMask.");
}

QPoint SendButton::prepareRippleStartPosition() const {
	const auto real = mapFromGlobal(QCursor::pos());
	switch (_lastRippleShape) {
	case RippleShape::InnerEllipse: {
		const auto size = _st.inner.rippleAreaSize;
		const auto y = (height() - size) / 2;
		return real - QPoint((width() - size) / 2, y);
	}
	case RippleShape::SendEllipse:
		return real - sendEllipseRect().topLeft();
	case RippleShape::StarsRoundRect:
		return real - starsGeometry().rounded.topLeft();
	case RippleShape::ScheduleEllipse:
		return real - scheduleEllipseRect().topLeft();
	}
	Unexpected("RippleShape in SendButton::prepareRippleStartPosition.");
}

void SendButton::initVoiceRoundIcon(int index) {
	Expects(index >= 0 && index < 2);

	_voiceRoundIcons[index] = Lottie::MakeIcon({
		.path = ((index == kVoiceToRoundIndex)
			? u":/animations/chat/voice_to_video.tgs"_q
			: u":/animations/chat/video_to_voice.tgs"_q),
		.sizeOverride = _st.recordSize,
		.colorizeUsingAlpha = true,
	});
}

void SendButton::paintVoiceRoundIcon(QPainter &p, bool over) {
	if (!isDisabled() && !_state.forbidden) {
		paintRipple(
			p,
			(width() - _st.inner.rippleAreaSize) / 2,
			_st.inner.rippleAreaPosition.y());
	}

	if (_state.forbidden) {
		p.setOpacity(kForbiddenOpacity);
	}
	const auto color = (isDisabled() || !over)
		? st::historyRecordVoiceFg->c
		: st::historyRecordVoiceFgOver->c;
	const auto toVideo = (_state.type == Type::Round);
	const auto index = toVideo ? kVoiceToRoundIndex : kRoundToVoiceIndex;
	_voiceRoundIcons[index]->paintInCenter(p, rect(), color);
	if (_state.forbidden) {
		p.setOpacity(1.);
	}
}

bool SendButton::isVoiceRoundTransition(Type from, Type to) {
	return (from == Type::Record && to == Type::Round)
		|| (from == Type::Round && to == Type::Record);
}

SendStarButton::SendStarButton(
	QWidget *parent,
	const style::IconButton &st,
	const style::RoundButton &counterSt,
	rpl::producer<SendStarButtonState> state)
: RippleButton(parent, st.ripple)
, _st(st)
, _counterSt(counterSt) {
	resize(_st.width, _st.height);

	std::move(state) | rpl::on_next([=](SendStarButtonState value) {
		setCount(value.count);
		highlight(value.highlight);
	}, lifetime());
}

void SendStarButton::paintEvent(QPaintEvent *e) {
	const auto ratio = style::DevicePixelRatio();
	const auto fullSize = size() * ratio;
	if (_frame.size() != fullSize) {
		_frame = QImage(fullSize, QImage::Format_ARGB32_Premultiplied);
		_frame.setDevicePixelRatio(ratio);
	}
	_frame.fill(Qt::transparent);

	auto p = QPainter(&_frame);

	const auto highlighted = _highlight.value(_highlighted ? 1. : 0.);
	p.setPen(Qt::NoPen);
	p.setBrush(_counterSt.textBg);
	p.drawEllipse(rect());
	paintRipple(p, QPoint());
	if (highlighted > 0.) {
		auto hq = PainterHighQualityEnabler(p);
		p.setBrush(st::creditsBg3);
		p.setOpacity(highlighted);
		p.drawEllipse(rect());
		p.setOpacity(1.);
	}

	st::starIconEmoji.icon.paintInCenter(p, rect(), st::premiumButtonFg->c);

	if (!_starsText.isEmpty()) {
		auto hq = PainterHighQualityEnabler(p);
		p.setCompositionMode(QPainter::CompositionMode_Source);
		auto pen = st::transparent->p;
		pen.setWidthF(_counterSt.numbersSkip);
		p.setPen(pen);
		p.setBrush(
			anim::brush(_counterSt.textBg, st::creditsBg3, highlighted));
		const auto size = QSize(
			_starsText.maxWidth(),
			_counterSt.style.font->height);
		const auto larger = size.grownBy(_counterSt.padding);
		const auto left = (width() - larger.width());
		const auto top = 0;
		const auto r = larger.height() / 2.;
		p.drawRoundedRect(left, top, larger.width(), larger.height(), r, r);
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		p.setPen(
			anim::pen(_counterSt.textFg, st::premiumButtonFg, highlighted));
		_starsText.draw(p, {
			.position = QPoint(
				left + _counterSt.padding.left(),
				top + _counterSt.padding.top()),
			.availableWidth = _starsText.maxWidth(),
		});
	}

	QPainter(this).drawImage(0, 0, _frame);
}

void SendStarButton::setCount(int count) {
	if (_count == count) {
		return;
	}
	_count = count;
	if (_count) {
		_starsText.setText(
			_counterSt.style,
			Lang::FormatCountDecimal(_count));
		const auto sub = _counterSt.padding.left()
			+ _counterSt.padding.right();
		if (_starsText.maxWidth() > width() - sub) {
			_starsText.setText(
				_counterSt.style,
				Lang::FormatCountToShort(_count).string);
		}
	} else {
		_starsText = Text::String();
	}
	update();
}

void SendStarButton::highlight(bool enabled) {
	if (_highlighted == enabled) {
		return;
	}
	_highlighted = enabled;
	_highlight.start(
		[=] { update(); },
		enabled ? 0. : 1.,
		enabled ? 1. : 0.,
		360,
		anim::easeOutCirc);
}

QImage SendStarButton::prepareRippleMask() const {
	return RippleAnimation::EllipseMask(size());
}

QPoint SendStarButton::prepareRippleStartPosition() const {
	return mapFromGlobal(QCursor::pos());
}

} // namespace Ui
