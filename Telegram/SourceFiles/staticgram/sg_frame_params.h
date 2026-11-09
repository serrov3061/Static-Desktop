/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <optional>

namespace StaticGram::FrameParams {

// Avatar frames v2 settings (Android FrameParams, frames-demo/CONTRACT.md).
// Wire form: profile list "frame_params": {"palette":"blue","speed":"1.5"}
// (strings) and the badge bot query "frame <style> key=value ...".
// A missing key is the default; unknown keys and values are ignored.

inline constexpr auto kSpeed = "speed";
inline constexpr auto kAnim = "anim";
inline constexpr auto kGlow = "glow";
inline constexpr auto kPalette = "palette";
inline constexpr auto kMetal = "metal";
inline constexpr auto kGem = "gem";
inline constexpr auto kGemPos = "gempos";
inline constexpr auto kDefaultGlow = 70;

using Values = QMap<QString, QString>;

// Picker order, the same as the server's.
[[nodiscard]] const QStringList &Styles();
[[nodiscard]] bool IsKnownStyle(const QString &style);
[[nodiscard]] const QStringList &Speeds();

[[nodiscard]] QString PaletteKey(const QString &style);
[[nodiscard]] QStringList PaletteValues(const QString &style);
[[nodiscard]] QStringList ToggleKeys(const QString &style);

// Values allowed for the key of the style, nullopt for unknown keys
// (and for glow, which is a number).
[[nodiscard]] std::optional<QStringList> AllowedValues(
	const QString &style,
	const QString &key);
[[nodiscard]] QString DefaultValue(const QString &style, const QString &key);

// Keeps only the keys and values the style accepts.
[[nodiscard]] Values Sanitize(const QString &style, const Values &raw);
// The sanitized values without the default ones (what is sent and stored).
[[nodiscard]] Values NonDefault(const QString &style, const Values &values);
// "frame <style> key=value ..." with non-default keys, "frame none".
[[nodiscard]] QString ToCommand(const QString &style, const Values &values);

} // namespace StaticGram::FrameParams
