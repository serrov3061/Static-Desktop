/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_balance_pill.h"

#include "data/data_user.h"
#include "info/channel_statistics/earn/earn_icons.h"
#include "lang/lang_keys.h"
#include "staticgram/sg_money_api.h"
#include "ui/controls/ton_common.h"
#include "ui/effects/credits_graphics.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"

#include "styles/style_info_profile_top_bar.h"

namespace Info::Profile {
namespace {

[[nodiscard]] int FixedWidth() {
	const auto &padding = st::infoProfileBalancePillPadding;
	return padding.left()
		+ st::infoProfileBalancePillIconSize
		+ st::infoProfileBalancePillIconSkip
		+ st::infoProfileBalancePillArrowSkip
		+ st::infoProfileBalancePillArrow.width()
		+ padding.right();
}

// StaticGram: the amounts of the card sheet (big ones abbreviated), the
// GRAM fraction is drawn with the smaller font.
[[nodiscard]] Wallet::AmountParts BalanceParts(
		StaticGram::Money::Kind kind,
		int64 balance) {
	const auto text = StaticGram::Money::FormatAmount(kind, balance);
	if (kind != StaticGram::Money::Kind::Gram) {
		return { .whole = text };
	}
	const auto index = text.indexOf(Ui::TonAmountSeparator());
	return (index < 0)
		? Wallet::AmountParts{ .whole = text }
		: Wallet::AmountParts{
			.whole = text.mid(0, index),
			.fraction = text.mid(index),
		};
}

} // namespace

BalancePill::BalancePill(
	QWidget *parent,
	not_null<UserData*> user,
	rpl::producer<std::optional<QColor>> edgeColor,
	StaticGram::Money::Kind kind)
: AbstractButton(parent)
, _kind(kind)
, _colors(ComputePillColors(std::nullopt))
, _baseline(PillTextBaseline())
, _arrowTop(PillArrowTop()) {
	setAttribute(Qt::WA_TransparentForMouseEvents);
	hide();

	std::move(
		edgeColor
	) | rpl::on_next([=](std::optional<QColor> color) {
		if (_edgeColor != color || _icon.isNull()) {
			_edgeColor = color;
			refreshColors();
		}
	}, lifetime());

	style::PaletteChanged(
	) | rpl::on_next([=] {
		refreshColors();
	}, lifetime());

	// StaticGram (Android ProfileBalanceView): GRAM and Stars are two
	// separate pills without the «Balance:» label, both shown once the
	// user has any sgcard, each opens its own card. Like Android
	// sgExpectsBalance, bots and service accounts are not asked.
	if (user->isBot()
		|| user->isInaccessible()
		|| user->isRepliesChat()
		|| user->isVerifyCodes()) {
		return;
	}
	StaticGram::Money::CardsValue(
		user
	) | rpl::on_next([=](const StaticGram::Money::Cards &cards) {
		const auto card = cards.find(_kind);
		setContent(
			QString(),
			card ? card->balance : int64(),
			cards.loaded && !cards.list.empty());
	}, lifetime());
}

StaticGram::Money::Kind BalancePill::kind() const {
	return _kind;
}

void BalancePill::setOpacity(float64 opacity) {
	if (_opacity != opacity) {
		_opacity = opacity;
		++_appearanceRevision;
		update();
	}
	const auto transparent = (_opacity <= 0.);
	if (testAttribute(Qt::WA_TransparentForMouseEvents) != transparent) {
		setAttribute(Qt::WA_TransparentForMouseEvents, transparent);
		if (transparent) {
			clearState();
		}
	}
}

int BalancePill::fittedWidth() const {
	return _fittedWidth;
}

bool BalancePill::actionable() const {
	return !isHidden() && (_opacity > 0.);
}

uint64 BalancePill::appearanceRevision() const {
	return _appearanceRevision;
}

QImage BalancePill::grabEmpty() {
	const auto was = std::exchange(_contentHidden, true);
	const auto restore = gsl::finally([&] {
		_contentHidden = was;
	});
	return Ui::GrabWidgetToImage(parentWidget(), geometry());
}

void BalancePill::paintContentAlone(QPainter &p) const {
	if (_opacity <= 0.) {
		return;
	}
	auto hq = PainterHighQualityEnabler(p);
	const auto was = p.opacity();
	p.setOpacity(was * _opacity);
	paintContent(p);
	p.setOpacity(was);
}

void BalancePill::setContent(
		const QString &label,
		int64 balance,
		bool shown) {
	if (!shown) {
		if (!isHidden()) {
			++_appearanceRevision;
		}
		hide();
		return;
	}
	const auto parts = BalanceParts(_kind, std::max(balance, int64()));
	if (isHidden()
		|| _label != label
		|| _amount.parts().whole != parts.whole
		|| _amount.parts().fraction != parts.fraction) {
		++_appearanceRevision;
	}
	_label = label;
	_amount.setContent({
		.big = st::infoProfileBalancePillAmountFont,
		.small = st::infoProfileBalancePillFractionFont,
	}, parts);
	const auto labelSpace = _label.isEmpty()
		? 0
		: (st::infoProfileBalancePillLabelFont->width(_label)
			+ st::infoProfileBalancePillLabelSkip);
	setNaturalWidth(FixedWidth() + labelSpace + _amount.naturalWidth());
	refreshLayout(width());
	accessibilityNameChanged();
	update();
	show();
}

void BalancePill::refreshColors() {
	++_appearanceRevision;
	_colors = ComputePillColors(_edgeColor);
	const auto tint = _edgeColor
		? Ui::BlendColors(
			*_edgeColor,
			_colors.primary,
			st::infoProfileBalancePillColoredIconTint)
		: st::currencyFg->c;
	_icon = (_kind == StaticGram::Money::Kind::Stars)
		? Ui::GenerateStars(st::infoProfileBalancePillIconSize, 1)
		: Ui::Earn::IconCurrencyTwoTone(
			st::infoProfileBalancePillIconSize,
			tint);
	const auto &font = st::infoProfileBalancePillAmountFont;
	const auto ratio = style::DevicePixelRatio();
	const auto top = _baseline
		- font->ascent
		+ Ui::Earn::AlignedMarkTop(font, _icon);
	_iconTop = std::round(top * ratio) / ratio;
	update();
}

int BalancePill::resizeGetHeight(int newWidth) {
	refreshLayout(newWidth);
	return st::infoProfileBalancePillHeight;
}

void BalancePill::refreshLayout(int newWidth) {
	const auto &font = st::infoProfileBalancePillLabelFont;
	const auto labelSkip = st::infoProfileBalancePillLabelSkip;
	const auto available = newWidth - FixedWidth();
	const auto labelAvailable = available
		- _amount.naturalWidth()
		- labelSkip;
	_labelShown = (font->width(_label) <= labelAvailable)
		? _label
		: (labelAvailable >= font->elidew)
		? font->elided(_label, labelAvailable)
		: QString();
	_labelSpace = _labelShown.isEmpty()
		? 0
		: (font->width(_labelShown) + labelSkip);
	const auto amountAvailable = std::max(available - _labelSpace, 1);
	_amount.setAvailableWidth(amountAvailable);
	_fittedWidth = std::min(
		newWidth,
		(FixedWidth()
			+ _labelSpace
			+ std::min(_amount.naturalWidth(), amountAvailable)));
}

void BalancePill::paintEvent(QPaintEvent *e) {
	if (_opacity <= 0.) {
		return;
	}
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);
	p.setOpacity(_opacity);
	PaintPillBackground(p, rect(), _colors);
	if (!_contentHidden) {
		paintContent(p);
	}
}

void BalancePill::paintContent(QPainter &p) const {
	const auto &fg = _colors.primary;
	const auto &padding = st::infoProfileBalancePillPadding;
	if (!_labelShown.isEmpty()) {
		p.setFont(st::infoProfileBalancePillLabelFont);
		p.setPen(_colors.secondary);
		p.drawText(padding.left(), _baseline, _labelShown);
	}
	const auto iconLeft = padding.left() + _labelSpace;
	p.drawImage(QPointF(iconLeft, _iconTop), _icon);

	const auto amountLeft = iconLeft
		+ st::infoProfileBalancePillIconSize
		+ st::infoProfileBalancePillIconSkip;
	const auto amountTop = _baseline - _amount.baseline() * _amount.scale();
	_amount.paint(p, QPointF(amountLeft, amountTop), { .digits = fg });

	const auto &arrow = st::infoProfileBalancePillArrow;
	arrow.paint(
		p,
		width() - padding.right() - arrow.width(),
		_arrowTop,
		width(),
		fg);
}

QString BalancePill::accessibilityName() {
	const auto &parts = _amount.parts();
	return tr::lng_wallet_send_balance(
		tr::now,
		lt_amount,
		parts.whole
			+ parts.fraction
			+ ((_kind == StaticGram::Money::Kind::Stars)
				? u" Stars"_q
				: u" GRAM"_q));
}

} // namespace Info::Profile
