/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_local_settings.h"

#include "settings.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace StaticGram::LocalSettings {
namespace {

constexpr auto kMaxFileSize = 64 * 1024;

[[nodiscard]] QString FilePath() {
	return cWorkingDir() + u"tdata/sg_settings.json"_q;
}

[[nodiscard]] QJsonObject &Values() {
	static auto result = [] {
		auto file = QFile(FilePath());
		if (!file.open(QIODevice::ReadOnly)) {
			return QJsonObject();
		}
		return QJsonDocument::fromJson(file.read(kMaxFileSize)).object();
	}();
	return result;
}

void Write() {
	QDir().mkpath(cWorkingDir() + u"tdata"_q);
	auto file = QFile(FilePath());
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		file.write(QJsonDocument(Values()).toJson(QJsonDocument::Compact));
	}
}

} // namespace

bool Bool(const QString &key, bool defaultValue) {
	const auto value = Values().value(key);
	return value.isBool() ? value.toBool() : defaultValue;
}

void SetBool(const QString &key, bool value) {
	auto &values = Values();
	const auto now = values.value(key);
	if (now.isBool() && now.toBool() == value) {
		return;
	}
	values.insert(key, value);
	Write();
}

bool ProfileGiftBubbles() {
	return Bool(QString::fromLatin1(kProfileGiftBubbles), true);
}

void SetProfileGiftBubbles(bool value) {
	SetBool(QString::fromLatin1(kProfileGiftBubbles), value);
}

} // namespace StaticGram::LocalSettings
