/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "staticgram/sg_money_api.h"

namespace Window {
class SessionController;
} // namespace Window

namespace StaticGram::Money {

// The own GRAM / Stars card editor (Android SGCardEditorActivity): the
// background is any own unique gift, stickers are own unique gifts and
// static stickers, moved / scaled / rotated right on the card, up to
// kMaxStickers, saved with sgcard.saveCard. Without a purchase it shows
// the @StaticGramApp purchase prompt instead.
void ShowCardEditor(
	not_null<Window::SessionController*> controller,
	Kind kind);

// Android SGCardsActivity.buildPromo: «Купить в @StaticGramApp».
void ShowCardsPurchaseBox(not_null<Window::SessionController*> controller);

} // namespace StaticGram::Money
