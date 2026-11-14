// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 08a6ed2a
// Mokagram: единый реестр настроек клиента, хранение в tdata/moka_settings.json.
#include "mokagram/settings/mokagram_settings.h"

#include "base/flat_map.h"
#include "base/timer.h"
#include "logs.h"
#include "settings.h"

#include <rpl/event_stream.h>
#include <rpl/filter.h>
#include <rpl/map.h>
#include <rpl/then.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>


namespace Mokagram::Settings {
namespace {

constexpr auto kSaveDelay = crl::time(300);

// StaticGram: only the brought back mokaGram features keep their values,
// in a file of our own, so a value left by an old mokaGram install can not
// switch a removed feature on.
[[nodiscard]] QString FilePath() {
	return cWorkingDir() + u"tdata/sg_moka_settings.json"_q;
}

[[nodiscard]] bool Persistent(const QString &key) {
	static const auto prefixes = {
		u"streamer."_q,
		u"lag."_q,
		u"music."_q,
		u"quote."_q,
		u"player."_q,
		u"lyrics."_q,
	};
	for (const auto &prefix : prefixes) {
		if (key.startsWith(prefix)) {
			return true;
		}
	}
	return false;
}

struct State {
	std::vector<Entry> entries;
	base::flat_map<QString, int> entryIndices;
	// Явно заданные значения: отличия от умолчаний и неизвестные ключи.
	base::flat_map<QString, QJsonValue> values;
	rpl::event_stream<QString> changed;
	base::Timer saveTimer;
	bool dirty = false;
	bool loaded = false;
	bool quitHooked = false;
};

[[nodiscard]] State &Data() {
	static auto result = State();
	return result;
}


[[nodiscard]] const Entry *Find(const QString &key) {
	const auto &data = Data();
	const auto i = data.entryIndices.find(key);
	return (i != data.entryIndices.end()) ? &data.entries[i->second] : nullptr;
}

// Приводит значение к виду, допустимому для записи. Пусто, если не подходит.
[[nodiscard]] std::optional<QJsonValue> Normalize(
		const Entry &entry,
		const QJsonValue &value) {
	switch (entry.kind) {
	case Kind::Bool:
		if (value.isBool()) {
			return value;
		}
		break;
	case Kind::Int:
		if (value.isDouble()) {
			const auto number = value.toInt();
			if (number >= entry.minValue && number <= entry.maxValue) {
				return QJsonValue(number);
			}
		}
		break;
	case Kind::Choice:
		if (value.isDouble()) {
			const auto number = value.toInt();
			if (number >= 0 && number < int(entry.choices.size())) {
				return QJsonValue(number);
			}
		}
		break;
	case Kind::String:
	case Kind::Color:
		if (value.isString()) {
			return value;
		}
		break;
	}
	return std::nullopt;
}

[[nodiscard]] bool SameValue(const QJsonValue &a, const QJsonValue &b) {
	return (a.type() == b.type())
		&& (a.isBool()
			? a.toBool() == b.toBool()
			: a.isString()
			? a.toString() == b.toString()
			: a.toDouble() == b.toDouble());
}

[[nodiscard]] QJsonValue Current(const QString &key) {
	auto &data = Data();
	if (const auto i = data.values.find(key); i != data.values.end()) {
		return i->second;
	}
	if (const auto entry = Find(key)) {
		return entry->defaultValue;
	}
	return QJsonValue();
}

void ScheduleSave() {
	auto &data = Data();
	data.dirty = true;
	if (data.loaded && !data.saveTimer.isActive()) {
		data.saveTimer.callOnce(kSaveDelay);
	}
}

void Store(const QString &key, const QJsonValue &value) {
	auto &data = Data();
	const auto entry = Find(key);
	if (!entry) {
		return;
	}
	const auto fixed = Normalize(*entry, value);
	if (!fixed) {
		LOG(("[mokagram] Settings: bad value for '%1'.").arg(key));
		return;
	}
	const auto was = Current(key);
	if (SameValue(*fixed, entry->defaultValue)) {
		data.values.remove(key);
	} else {
		data.values[key] = *fixed;
	}
	if (SameValue(was, Current(key))) {
		return;
	}
	ScheduleSave();
	data.changed.fire_copy(key);
}

void ResetKey(const QString &key) {
	auto &data = Data();
	const auto entry = Find(key);
	if (!entry || !data.values.contains(key)) {
		return;
	}
	data.values.remove(key);
	ScheduleSave();
	data.changed.fire_copy(key);
}



} // namespace

void Register(Entry entry) {
	auto &data = Data();
	const auto key = entry.key;
	const auto index = data.entryIndices.find(key);
	if (index != data.entryIndices.end()) {
		data.entries[index->second] = std::move(entry);
	} else {
		data.entryIndices.emplace(key, int(data.entries.size()));
		data.entries.push_back(std::move(entry));
	}
	// Значение из файла могло прийти раньше записи: проверяем его теперь.
	const auto &now = *Find(key);
	if (const auto i = data.values.find(key); i != data.values.end()) {
		const auto fixed = Normalize(now, i->second);
		if (!fixed || SameValue(*fixed, now.defaultValue)) {
			if (!fixed) {
				LOG(("[mokagram] Settings: bad stored value for '%1'."
					).arg(key));
			}
			data.values.remove(key);
			ScheduleSave();
		} else if (!SameValue(*fixed, i->second)) {
			i->second = *fixed;
		}
	}
}

const std::vector<Entry> &Entries() {
	return Data().entries;
}

bool Bool(const QString &key) {
	const auto value = Current(key);
	return value.isBool() ? value.toBool() : false;
}

int Int(const QString &key) {
	const auto value = Current(key);
	return value.isDouble() ? value.toInt() : 0;
}

QString String(const QString &key) {
	const auto value = Current(key);
	return value.isString() ? value.toString() : QString();
}

void SetBool(const QString &key, bool value) {
	Store(key, value);
}

void SetInt(const QString &key, int value) {
	Store(key, value);
}

void SetString(const QString &key, const QString &value) {
	Store(key, value);
}

void Reset(const QString &key) {
	ResetKey(key);
}

void ResetAll() {
	auto keys = QStringList();
	for (const auto &entry : Data().entries) {
		keys.push_back(entry.key);
	}
	for (const auto &key : keys) {
		ResetKey(key);
	}
}

void ResetSection(const QString &section) {
	auto keys = QStringList();
	for (const auto &entry : Data().entries) {
		if (entry.section == section) {
			keys.push_back(entry.key);
		}
	}
	for (const auto &key : keys) {
		ResetKey(key);
	}
}

bool IsChanged(const QString &key) {
	if (const auto entry = Find(key); entry && entry->action) {
		return false;
	}
	return Data().values.contains(key);
}

rpl::producer<bool> BoolValue(const QString &key) {
	return rpl::single(
		Bool(key)
	) | rpl::then(
		Data().changed.events()
		| rpl::filter([=](const QString &changed) { return changed == key; })
		| rpl::map([=](const QString &) { return Bool(key); }));
}

rpl::producer<int> IntValue(const QString &key) {
	return rpl::single(
		Int(key)
	) | rpl::then(
		Data().changed.events()
		| rpl::filter([=](const QString &changed) { return changed == key; })
		| rpl::map([=](const QString &) { return Int(key); }));
}

rpl::producer<QString> StringValue(const QString &key) {
	return rpl::single(
		String(key)
	) | rpl::then(
		Data().changed.events()
		| rpl::filter([=](const QString &changed) { return changed == key; })
		| rpl::map([=](const QString &) { return String(key); }));
}

rpl::producer<QString> Changed() {
	return Data().changed.events();
}

void Load() {
	auto &data = Data();
	if (!data.quitHooked) {
		data.quitHooked = true;
		data.saveTimer.setCallback([] { Save(); });
		QObject::connect(
			QCoreApplication::instance(),
			&QCoreApplication::aboutToQuit,
			[] { Save(); });
	}
	data.values.clear();
	auto file = QFile(FilePath());
	if (file.open(QIODevice::ReadOnly)) {
		auto error = QJsonParseError();
		const auto document = QJsonDocument::fromJson(
			file.readAll(),
			&error);
		if (error.error != QJsonParseError::NoError
			|| !document.isObject()) {
			LOG(("[mokagram] Settings: sg_moka_settings.json is broken."));
		} else {
			const auto values = document.object().value(
				u"values"_q).toObject();
			for (auto i = values.begin(); i != values.end(); ++i) {
				if (Persistent(i.key())) {
					data.values[i.key()] = i.value();
				}
			}
		}
	}
	for (const auto &entry : data.entries) {
		const auto i = data.values.find(entry.key);
		if (i == data.values.end()) {
			continue;
		}
		const auto fixed = Normalize(entry, i->second);
		if (!fixed || SameValue(*fixed, entry.defaultValue)) {
			data.values.remove(entry.key);
		} else {
			i->second = *fixed;
		}
	}
	data.loaded = true;
	data.dirty = false;
}

void Save() {
	auto &data = Data();
	data.saveTimer.cancel();
	if (!data.dirty || !data.loaded) {
		return;
	}
	data.dirty = false;
	auto values = QJsonObject();
	for (const auto &[key, value] : data.values) {
		if (Persistent(key)) {
			values.insert(key, value);
		}
	}
	auto root = QJsonObject();
	root.insert(u"version"_q, 1);
	root.insert(u"values"_q, values);
	auto file = QSaveFile(FilePath());
	if (!file.open(QIODevice::WriteOnly)) {
		LOG(("[mokagram] Settings: cannot write sg_moka_settings.json."));
		data.dirty = true;
		return;
	}
	file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
	if (!file.commit()) {
		LOG(("[mokagram] Settings: commit of sg_moka_settings.json failed."));
		data.dirty = true;
	}
}

QByteArray Export() {
	return QByteArray();
}

bool PreviewImport(
		const QByteArray &,
		ImportSummary *,
		QString *error) {
	if (error) {
		*error = QString();
	}
	return false;
}

bool Import(const QByteArray &, QString *error) {
	if (error) {
		*error = QString();
	}
	return false;
}

} // namespace Mokagram::Settings
