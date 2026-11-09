/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_proxies.h"

#include "core/application.h"
#include "main/main_account.h"
#include "main/main_app_config.h"
#include "main/main_domain.h"
#include "settings.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>

namespace StaticGram::Proxies {
namespace {

constexpr auto kMaxCacheSize = qint64(256 * 1024);

struct Lists {
	std::vector<OfficialProxy> proxies;
	std::vector<QString> hosts; // Lowercase.

	friend inline bool operator==(const Lists &a, const Lists &b) {
		if (a.hosts != b.hosts || a.proxies.size() != b.proxies.size()) {
			return false;
		}
		for (auto i = 0, count = int(a.proxies.size()); i != count; ++i) {
			const auto &x = a.proxies[i];
			const auto &y = b.proxies[i];
			if (x.title != y.title
				|| x.server != y.server
				|| x.port != y.port
				|| x.secret != y.secret) {
				return false;
			}
		}
		return true;
	}
};

[[nodiscard]] QString CachePath() {
	return cWorkingDir() + u"tdata/sg_proxies.json"_q;
}

[[nodiscard]] QString Lower(const QString &host) {
	return host.trimmed().toLower();
}

void AddHost(std::vector<QString> &hosts, const QString &host) {
	const auto lower = Lower(host);
	if (!lower.isEmpty() && !ranges::contains(hosts, lower)) {
		hosts.push_back(lower);
	}
}

[[nodiscard]] std::optional<OfficialProxy> Parse(
		const QString &title,
		const QString &server,
		qint64 port,
		const QString &secret) {
	const auto host = server.trimmed();
	if (host.isEmpty() || port <= 0 || port > 65535) {
		return std::nullopt;
	}
	const auto name = title.trimmed();
	return OfficialProxy{
		.title = name.isEmpty() ? host : name,
		.server = host,
		.port = uint32(port),
		.secret = secret.trimmed(),
	};
}

[[nodiscard]] Lists ReadCache() {
	auto result = Lists();
	auto file = QFile(CachePath());
	if (!file.open(QIODevice::ReadOnly)) {
		return result;
	}
	const auto document = QJsonDocument::fromJson(file.read(kMaxCacheSize));
	const auto root = document.object();
	for (const auto &value : root.value(u"proxies"_q).toArray()) {
		const auto object = value.toObject();
		if (const auto proxy = Parse(
				object.value(u"title"_q).toString(),
				object.value(u"server"_q).toString(),
				object.value(u"port"_q).toInteger(),
				object.value(u"secret"_q).toString())) {
			result.proxies.push_back(*proxy);
		}
	}
	for (const auto &value : root.value(u"hosts"_q).toArray()) {
		AddHost(result.hosts, value.toString());
	}
	return result;
}

void WriteCache(const Lists &lists) {
	auto proxies = QJsonArray();
	for (const auto &proxy : lists.proxies) {
		proxies.push_back(QJsonObject{
			{ u"title"_q, proxy.title },
			{ u"server"_q, proxy.server },
			{ u"port"_q, qint64(proxy.port) },
			{ u"secret"_q, proxy.secret },
		});
	}
	auto hosts = QJsonArray();
	for (const auto &host : lists.hosts) {
		hosts.push_back(host);
	}
	QDir().mkpath(cWorkingDir() + u"tdata"_q);
	auto file = QFile(CachePath());
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		file.write(QJsonDocument(QJsonObject{
			{ u"proxies"_q, proxies },
			{ u"hosts"_q, hosts },
		}).toJson(QJsonDocument::Compact));
	}
}

[[nodiscard]] Lists FromConfig(const Main::AppConfig &config) {
	auto result = Lists();
	for (const auto &object : config.getObjectArray(
			u"sg_official_proxies"_q)) {
		const auto value = [&](const QString &key) {
			const auto i = object.find(key);
			return (i != end(object)) ? i->second : QString();
		};
		if (const auto proxy = Parse(
				value(u"title"_q),
				value(u"server"_q),
				value(u"port"_q).toLongLong(),
				value(u"secret"_q))) {
			result.proxies.push_back(*proxy);
		}
	}
	for (const auto &host : config.get<std::vector<QString>>(
			u"sg_official_proxy_hosts"_q,
			std::vector<QString>())) {
		AddHost(result.hosts, host);
	}
	for (const auto &proxy : result.proxies) {
		AddHost(result.hosts, proxy.server);
	}
	return result;
}

// The app config of a logged in account wins; the cache is used before the
// first login (or while the config has none of the keys).
[[nodiscard]] const Lists &Current() {
	static auto cached = std::optional<Lists>();
	if (!cached) {
		cached = ReadCache();
	}
	if (Core::IsAppLaunched() && Core::App().domain().started()) {
		const auto &account = Core::App().activeAccount();
		if (account.sessionExists()) {
			auto fresh = FromConfig(account.appConfig());
			if (!fresh.proxies.empty() || !fresh.hosts.empty()) {
				if (!(fresh == *cached)) {
					WriteCache(fresh);
					cached = std::move(fresh);
				}
			}
		}
	}
	return *cached;
}

} // namespace

std::vector<OfficialProxy> OfficialList() {
	return Current().proxies;
}

MTP::ProxyData ToProxyData(const OfficialProxy &proxy) {
	auto result = MTP::ProxyData();
	result.host = proxy.server;
	result.port = proxy.port;
	if (proxy.secret.isEmpty()) {
		result.type = MTP::ProxyData::Type::Socks5;
	} else {
		result.type = MTP::ProxyData::Type::Mtproto;
		result.password = proxy.secret;
	}
	return result;
}

bool IsOfficialHost(const QString &host) {
	return ranges::contains(Current().hosts, Lower(host));
}

bool NeedsWarning(const MTP::ProxyData &proxy) {
	using Type = MTP::ProxyData::Type;
	if (proxy.type == Type::Mtproto) {
		return !IsOfficialHost(proxy.host);
	} else if (proxy.type == Type::Web) {
		return !IsOfficialHost(proxy.webAddress().section('/', 0, 0));
	}
	return false;
}

QString TelegramLinkToLocal(const QString &url) {
	static const auto re = QRegularExpression(
		u"^(?:tg://|(?:https?://)?(?:www\\.)?"
		u"(?:t\\.me|telegram\\.me|telegram\\.dog)/)"
		u"(proxy|socks|webproxy)/?\\?(.+)$"_q,
		QRegularExpression::CaseInsensitiveOption);
	const auto match = re.match(url.trimmed());
	return match.hasMatch()
		? (u"sg://"_q + match.captured(1).toLower() + '?' + match.captured(2))
		: QString();
}

} // namespace StaticGram::Proxies
