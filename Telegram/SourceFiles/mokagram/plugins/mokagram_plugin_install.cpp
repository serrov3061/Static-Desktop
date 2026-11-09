// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 88dee111
// StaticGram: the mokaGram feature behind this API is removed; this is an
// inert stub that keeps the interface so the call sites compile unchanged.
#include "mokagram/plugins/mokagram_plugin_install.h"

#include "base/call_delayed.h"
#include "ui/layers/generic_box.h"

namespace Mokagram::Plugins {

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

} // namespace Mokagram::Plugins
