// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 1e0b9cfd
// Mokagram: colored icons of the settings rows, taken from a custom emoji
// set (moved out of the removed «Кастомизация» box by StaticGram).
#pragma once

#include "base/basic_types.h"

#include <rpl/producer.h>

#include <memory>

namespace Ui::Text {
class CustomEmoji;
} // namespace Ui::Text

namespace style::internal {
class Icon;
} // namespace style::internal

namespace style {
using icon = internal::Icon;
} // namespace style

namespace Mokagram::Settings {

// Short name of the custom emoji set the settings row icons come from.
inline constexpr auto kRowIconsSet = "iOSTelegram";

// Kept for the settings row code: a plain icon without a pack pair.
[[nodiscard]] const style::icon &OwnRowIcon();

// Pack emoji for a settings row icon; nullptr, if the icon has no pair
// in the set or the set is not loaded yet.
[[nodiscard]] std::unique_ptr<Ui::Text::CustomEmoji> RowIconEmoji(
	const void *icon,
	Fn<void()> repaint,
	int size);
[[nodiscard]] std::unique_ptr<Ui::Text::CustomEmoji> AltIconEmoji(
	const QString &alt,
	Fn<void()> repaint,
	int size);
[[nodiscard]] rpl::producer<> RowIconsUpdated();

// Whether the icon has a pair in the set (the old icon is not painted
// while the set is loading).
[[nodiscard]] bool HasRowIcon(const void *icon);

} // namespace Mokagram::Settings
