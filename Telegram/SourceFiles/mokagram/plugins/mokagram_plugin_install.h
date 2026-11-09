// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 214e097e
// Mokagram: окно установки плагина при клике на файл .plugin в чате.
// StaticGram: the mokaGram plugin system is removed. Only the rounded box
// helper (design) and an inert first-run hook remain.
#pragma once

#include "base/basic_types.h"

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace style {
struct Box;
} // namespace style

namespace Mokagram::Plugins {

// Does nothing (the mokaGram welcome box is removed).
void MaybeShowWelcome(not_null<Window::SessionController*> controller);

// Скругляет обычное окно (радиус 28), оставляя его кнопки внизу.
void RoundedKeepButtons(
	not_null<Ui::GenericBox*> box,
	const style::Box &base);

} // namespace Mokagram::Plugins
