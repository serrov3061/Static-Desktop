// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 88dee111
// StaticGram: the mokaGram feature behind this API is removed; this is an
// inert stub that keeps the interface so the call sites compile unchanged.
#include "mokagram/plugins/mokagram_plugin_install.h"

#include "base/call_delayed.h"
#include "ui/layers/generic_box.h"
#include "ui/abstract_button.h"
#include "ui/painter.h"
#include "styles/style_layers.h"

namespace Mokagram::Plugins {
namespace {

void RoundedNoButtons(not_null<Ui::GenericBox*> box) {
	static const auto clear = style::owned_color(QColor(0, 0, 0, 0));
	static const auto noButtons = [] {
		auto result = st::defaultBox;
		result.buttonPadding = style::margins();
		result.buttonHeight = 0;
		result.bg = clear.color();
		return result;
	}();
	box->setStyle(noButtons);
	box->paintOn([=](QPainter &p) {
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(st::boxBg);
		p.drawRoundedRect(box->rect(), 28, 28);
	});
	box->setWidth(st::boxWideWidth);
	box->setNoContentMargin(true);
}

// Круглая кнопка шапки: крестик (1), бургер (2) или лупа (0).
[[nodiscard]] not_null<Ui::AbstractButton*> MakeRoundButton(
		not_null<QWidget*> parent,
		int kind,
		Fn<void()> callback) {
	const auto button = Ui::CreateChild<Ui::AbstractButton>(parent.get());
	button->resize(36, 36);
	button->setPointerCursor(true);
	button->paintRequest(
	) | rpl::on_next([=] {
		auto p = QPainter(button);
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(st::windowBg);
		p.drawEllipse(button->rect());
		p.scale(0.75, 0.75);
		auto pen = QPen(st::windowFg->c, 2.2);
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		if (kind == 1) {
			p.drawLine(QPointF(17, 17), QPointF(31, 31));
			p.drawLine(QPointF(31, 17), QPointF(17, 31));
		} else if (kind == 2) {
			p.drawLine(QPointF(15, 17.5), QPointF(33, 17.5));
			p.drawLine(QPointF(15, 24), QPointF(33, 24));
			p.drawLine(QPointF(15, 30.5), QPointF(33, 30.5));
		} else {
			p.drawEllipse(QPointF(22.5, 22.5), 7.5, 7.5);
			p.drawLine(QPointF(28, 28), QPointF(34, 34));
		}
	}, button->lifetime());
	button->setClickedCallback(std::move(callback));
	return button;
}

} // namespace

void MaybeShowWelcome(not_null<Window::SessionController*>) {
}

void RoundedKeepButtons(
		not_null<Ui::GenericBox*> box,
		const style::Box &base) {
	box->setStyle(base);
	// Скругление рисует сама подложка окна, вместе с кнопками внизу.
	const auto apply = [=] {
		if (const auto layer = box->parentWidget()) {
			layer->setProperty("moka-radius", 28);
			layer->update();
		}
	};
	apply();
	base::call_delayed(0, box, apply);
}

void RoundedWindow(not_null<Ui::GenericBox*> box) {
	RoundedNoButtons(box);
}

not_null<Ui::AbstractButton*> MakeHeaderButton(
		not_null<QWidget*> parent,
		bool close,
		Fn<void()> callback) {
	return MakeRoundButton(parent, close ? 1 : 0, std::move(callback));
}

} // namespace Mokagram::Plugins
