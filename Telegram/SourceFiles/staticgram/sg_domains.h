/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Window {
class SessionController;
} // namespace Window

// StaticGram .ton domains of our chain (server docs/ton-dns.md, private
// TL tondns.*; Android ui/Wallet/SGTonDns, SGDomainsActivity,
// SGDomainSheet, SGDomainSaleSheet): search «name.ton» with the price
// (shorter = more expensive, up to 32 of a-z 0-9 _), buy for GRAM, the own
// domains with the primary one, the wallet record and the resale.
namespace StaticGram::Domains {

inline constexpr auto kMaxName = 32;

// «Alice.TON», «@alice», « alice » -> «alice»; empty when not valid.
[[nodiscard]] QString Normalize(const QString &input);

// «Домены .ton»: the search, «Мои домены» and the domains for sale.
void ShowDomainsBox(not_null<Window::SessionController*> controller);

} // namespace StaticGram::Domains
