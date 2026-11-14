/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

// StaticGram client-side switches (Android MusicPlayerConfig-like), kept in
// tdata/sg_settings.json, independent of the account.
namespace StaticGram::LocalSettings {

// «Profile» → «Gift bubbles»: pinned gifts orbit the avatar in glass
// bubbles (on) or sit around it the standard way (off).
inline constexpr auto kProfileGiftBubbles = "profile_gift_bubbles";

[[nodiscard]] bool Bool(const QString &key, bool defaultValue);
void SetBool(const QString &key, bool value);

[[nodiscard]] bool ProfileGiftBubbles();
void SetProfileGiftBubbles(bool value);

} // namespace StaticGram::LocalSettings
