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

// «Деньги» (Android SGMoneyActivity): own GRAM / Stars cards, the send
// buttons and the «Transactions» / «Collectibles» tabs.
void ShowMoneyBox(
	not_null<Window::SessionController*> controller,
	Kind initial = Kind::Gram);

// A user's cards (Android SGCardSheet), from the profile or a sg://card
// link: balances, the send button, «Edit card» for one's own.
void ShowCardSheet(
	not_null<Window::SessionController*> controller,
	not_null<UserData*> user,
	Kind kind = Kind::Gram);

// sg://card?id=<uid>&kind=<0|1>: looks the user up when not known yet.
void ShowCardSheetById(
	not_null<Window::SessionController*> controller,
	uint64 userId,
	Kind kind);

} // namespace StaticGram::Money
