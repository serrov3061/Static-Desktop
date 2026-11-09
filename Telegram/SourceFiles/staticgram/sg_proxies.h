/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/mtproto_proxy_data.h"

// Official StaticGram proxies (Android MessagesController
// getOfficialProxies / isOfficialProxyHost): app config sg_official_proxies
// [{title, server, port, secret}] (an empty secret is SOCKS5) and
// sg_official_proxy_hosts. The last list seen is kept in tdata, so it is
// there before login too, when a proxy is needed most.
namespace StaticGram::Proxies {

struct OfficialProxy {
	QString title;
	QString server;
	uint32 port = 0;
	QString secret;
};

[[nodiscard]] std::vector<OfficialProxy> OfficialList();
[[nodiscard]] MTP::ProxyData ToProxyData(const OfficialProxy &proxy);
[[nodiscard]] bool IsOfficialHost(const QString &host);

// Telegram MTProxies forward only to Telegram servers: an MTProto (or web)
// proxy whose host is not one of ours most likely does not work here.
[[nodiscard]] bool NeedsWarning(const MTP::ProxyData &proxy);

// tg://proxy?…, t.me/proxy?… (socks, webproxy too) -> sg://proxy?…, so a
// pasted Telegram proxy link can be added (with the warning above).
[[nodiscard]] QString TelegramLinkToLocal(const QString &url);

} // namespace StaticGram::Proxies
