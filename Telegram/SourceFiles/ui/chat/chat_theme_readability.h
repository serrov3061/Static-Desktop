/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace style {
class palette;
} // namespace style

namespace Ui {

struct BubblesReadabilityArgs {
	bool dark = false;
	std::vector<QColor> outgoingGradient;
	bool deriveOutgoingSelection = false;
	const style::palette *reference = nullptr;
};

// With a reference palette the floors never exceed the reference's own.
void EnsureBubblesReadable(
	const style::palette &palette,
	const BubblesReadabilityArgs &args);

// StaticGram: darkens accent fills too light for the white text on them
// (a white / light grey accent). Also called for a palette from cache.
void EnsureAccentFillsReadable(const style::palette &palette);

[[nodiscard]] bool IsDarkPalette(const style::palette &palette);

[[nodiscard]] QColor EnsurePeerNameReadable(
	const QColor &color,
	const QColor &background,
	bool dark);

[[nodiscard]] std::vector<QColor> DarkenUnderWhiteText(
	std::vector<QColor> colors);

} // namespace Ui
