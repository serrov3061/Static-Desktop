/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Ui {
class GenericBox;
class RoundButton;
class VerticalLayout;
} // namespace Ui

namespace StaticGram::BoxParts {

// The mokaGram Material-3 look for the StaticGram boxes: a rounded layer,
// pill buttons and the rows grouped into rounded cards on the box bg.

void SetupBox(not_null<Ui::GenericBox*> box);

// A rounded card (mokaGram PaintCard) holding a group of rows; row hover
// highlights are clipped to the card corners.
[[nodiscard]] not_null<Ui::VerticalLayout*> AddCard(
	not_null<Ui::VerticalLayout*> container,
	int top = 6,
	int bottom = 6);

// A small caption above the rows inside a card.
void AddCardTitle(
	not_null<Ui::VerticalLayout*> card,
	rpl::producer<QString> text);

// The gray explanation under a card (instead of a divider with text).
void AddCardNote(
	not_null<Ui::VerticalLayout*> container,
	rpl::producer<QString> text);

// A full-width pill action button.
not_null<Ui::RoundButton*> AddMainButton(
	not_null<Ui::VerticalLayout*> container,
	rpl::producer<QString> text,
	Fn<void()> callback,
	int top = 10);

} // namespace StaticGram::BoxParts
