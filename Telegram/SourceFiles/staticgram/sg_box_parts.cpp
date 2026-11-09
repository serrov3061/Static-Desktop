/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_box_parts.h"

#include "mokagram/ui/mokagram_box_parts.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_layers.h"
#include "styles/style_sg_ui.h"

#include <QtGui/QPainterPath>

namespace StaticGram::BoxParts {
namespace {

using Mokagram::BoxParts::kCardRadius;
using Mokagram::BoxParts::kOuterSide;

constexpr auto kCardInset = 4;
constexpr auto kTextLeft = 16;

// The box bg outside a rounded corner, painted over the square corner of a
// row highlight (the way settings_builder.cpp clips its groups).
void PaintCorners(QPainter &p, QRect card, const QColor &bg) {
	const auto r = std::min(kCardRadius, card.height() / 2);
	if (r < 1) {
		return;
	}
	auto path = QPainterPath();
	path.addRect(QRectF(card));
	auto rounded = QPainterPath();
	rounded.addRoundedRect(QRectF(card), r, r);
	auto corners = QPainterPath();
	for (const auto right : { false, true }) {
		for (const auto bottom : { false, true }) {
			corners.addRect(QRectF(
				right ? (card.x() + card.width() - r) : card.x(),
				bottom ? (card.y() + card.height() - r) : card.y(),
				r,
				r));
		}
	}
	auto hq = PainterHighQualityEnabler(p);
	p.fillPath(corners.intersected(path.subtracted(rounded)), bg);
}

} // namespace

void SetupBox(not_null<Ui::GenericBox*> box) {
	Mokagram::BoxParts::RoundLayer(box);
	box->setStyle(Mokagram::BoxParts::PillButtonsBox());
}

not_null<Ui::VerticalLayout*> AddCard(
		not_null<Ui::VerticalLayout*> container,
		int top,
		int bottom) {
	const auto wrap = container->add(
		object_ptr<Ui::PaddingWrap<Ui::VerticalLayout>>(
			container,
			object_ptr<Ui::VerticalLayout>(container),
			QMargins(0, kCardInset, 0, kCardInset)),
		QMargins(kOuterSide, top, kOuterSide, bottom),
		// Full content width: a card holding only labels would otherwise
		// shrink to the labels' natural width.
		style::al_justify);
	wrap->paintOn([=](QPainter &p) {
		Mokagram::BoxParts::PaintCard(p, wrap->rect());
	});
	const auto corners = Ui::CreateChild<Ui::RpWidget>(wrap);
	corners->setAttribute(Qt::WA_TransparentForMouseEvents);
	corners->paintOn([=](QPainter &p) {
		PaintCorners(p, corners->rect(), st::boxBg->c);
	});
	wrap->sizeValue() | rpl::on_next([=](QSize size) {
		corners->setGeometry(QRect(QPoint(), size));
		corners->raise();
	}, corners->lifetime());
	return wrap->entity();
}

void AddCardTitle(
		not_null<Ui::VerticalLayout*> card,
		rpl::producer<QString> text) {
	card->add(
		object_ptr<Ui::FlatLabel>(card, std::move(text), st::sgCardTitle),
		QMargins(kTextLeft, 8, kTextLeft, 4));
}

void AddCardNote(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text) {
	container->add(
		object_ptr<Ui::FlatLabel>(container, std::move(text), st::sgCardNote),
		QMargins(kOuterSide + kTextLeft, 2, kOuterSide + kTextLeft, 8));
}

not_null<Ui::RoundButton*> AddMainButton(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		Fn<void()> callback,
		int top) {
	const auto &st = st::sgCardMainButton;
	const auto row = container->add(
		object_ptr<Ui::RpWidget>(container),
		QMargins(kOuterSide, top, kOuterSide, 6));
	row->resize(row->width(), st.height);
	const auto button = Ui::CreateChild<Ui::RoundButton>(
		row,
		std::move(text),
		st);
	button->setFullRadius(true);
	button->setTextTransform(Ui::RoundButtonTextTransform::NoTransform);
	button->setClickedCallback(std::move(callback));
	row->widthValue() | rpl::on_next([=](int width) {
		button->setFullWidth(width);
		button->moveToLeft(0, 0);
	}, row->lifetime());
	return button;
}

} // namespace StaticGram::BoxParts
