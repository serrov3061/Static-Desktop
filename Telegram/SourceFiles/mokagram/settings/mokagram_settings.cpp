// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 08a6ed2a
// Mokagram: единый реестр настроек клиента, хранение в tdata/moka_settings.json.
#include "mokagram/settings/mokagram_settings.h"

#include "base/flat_map.h"
#include "logs.h"

#include <rpl/event_stream.h>
#include <rpl/filter.h>
#include <rpl/map.h>
#include <rpl/then.h>


namespace Mokagram::Settings {
namespace {


struct State {
	std::vector<Entry> entries;
	base::flat_map<QString, int> entryIndices;
	// Явно заданные значения: отличия от умолчаний и неизвестные ключи.
	base::flat_map<QString, QJsonValue> values;
	rpl::event_stream<QString> changed;
	bool dirty = false;
	bool loaded = false;
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
	Data().dirty = true;
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

// StaticGram: the registry only serves the built-in design defaults now.
// Nothing is read from or written to tdata/moka_settings.json, so a value
// changed by an old mokaGram install can not switch a removed feature on.
void Load() {
	auto &data = Data();
	data.values.clear();
	data.loaded = true;
	data.dirty = false;
}

void Save() {
	auto &data = Data();
	data.dirty = false;
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
