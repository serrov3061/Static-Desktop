/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "staticgram/sg_money_api.h"

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace StaticGram::Money {

// The actions on one NFT of the custodial chain wallet (Android SGNftSheet
// and StarGiftSheet in wallet mode): «Передать» to a StaticGram user, «на
// адрес» and «Вернуть в профиль» for gifts, «Скопировать адрес». `moved`
// is called once a move was queued (the list should reload).
void FillNftMenu(
	not_null<Ui::PopupMenu*> menu,
	not_null<Window::SessionController*> controller,
	const NftItem &item,
	Fn<void()> moved);

// «Передать» of a .ton domain NFT: «Передать пользователю» (a StaticGram
// user) and «Отправить на адрес» (a wallet address or name.ton, resolved
// with tondns.resolve), both through gramwallet.transferNft.
void FillNftTransferMenu(
	not_null<Ui::PopupMenu*> menu,
	not_null<Window::SessionController*> controller,
	const NftItem &item,
	Fn<void()> moved);

} // namespace StaticGram::Money
