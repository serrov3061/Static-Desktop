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

// Android SGSendMoneyView: the recipient, the amount (GRAM with 9 decimals
// or whole Stars), an optional comment and one confirm button. Without a
// recipient a users list is shown first; for GRAM it starts with «На
// адрес» (a wallet address, a name.ton or a @username).
void ShowSendMoney(
	not_null<Window::SessionController*> controller,
	Kind kind,
	UserData *to = nullptr);

[[nodiscard]] bool CanReceiveMoney(not_null<UserData*> user);

} // namespace StaticGram::Money
