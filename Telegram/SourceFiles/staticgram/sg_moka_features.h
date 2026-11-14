/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Window {
class SessionController;
} // namespace Window

namespace StaticGram::MokaFeatures {

// The brought back mokaGram features (14.11.2026): streamer mode,
// anti-lag / background economy. Their values live in the
// Mokagram::Settings registry, this is the screen that edits them.

// Registers the settings of these features, once at start.
void RegisterSettings();

void ShowStreamerBox(not_null<Window::SessionController*> controller);
void ShowMusicSettingsBox(not_null<Window::SessionController*> controller);
void ShowPerformanceBox(not_null<Window::SessionController*> controller);

} // namespace StaticGram::MokaFeatures
