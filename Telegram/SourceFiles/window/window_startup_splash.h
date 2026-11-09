/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Ui {
class RpWidget;
} // namespace Ui

namespace Window {

// StaticGram: one-time animated logo overlay shown above the window body
// on application start. Removes itself after the animation finishes.
void ShowStartupSplash(not_null<Ui::RpWidget*> body);

} // namespace Window
