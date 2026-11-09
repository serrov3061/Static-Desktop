/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_frame_params.h"

namespace StaticGram::FrameParams {
namespace {

[[nodiscard]] int ParseGlowStrict(const QString &value) {
	auto ok = false;
	const auto result = value.trimmed().toInt(&ok);
	return (ok && result >= 0 && result <= 100) ? result : -1;
}

[[nodiscard]] const QStringList &OnOff() {
	static const auto result = QStringList{ u"1"_q, u"0"_q };
	return result;
}

} // namespace

const QStringList &Styles() {
	static const auto result = QStringList{
		u"inferno"_q,
		u"pixel"_q,
		u"solitaire"_q,
		u"cosmos"_q,
		u"frost"_q,
		u"neko"_q,
		u"angel"_q,
		u"devil"_q,
	};
	return result;
}

bool IsKnownStyle(const QString &style) {
	return !style.isEmpty() && Styles().contains(style);
}

const QStringList &Speeds() {
	static const auto result = QStringList{
		u"0.5"_q,
		u"1"_q,
		u"1.5"_q,
		u"2"_q,
	};
	return result;
}

QString PaletteKey(const QString &style) {
	return (style == u"solitaire"_q)
		? QString::fromLatin1(kMetal)
		: QString::fromLatin1(kPalette);
}

QStringList PaletteValues(const QString &style) {
	if (style == u"inferno"_q) {
		return { u"fire"_q, u"blue"_q, u"green"_q, u"purple"_q, u"pink"_q };
	} else if (style == u"pixel"_q) {
		return {
			u"rainbow"_q,
			u"pastel"_q,
			u"neon"_q,
			u"mono"_q,
			u"gold"_q,
		};
	} else if (style == u"solitaire"_q) {
		return { u"platinum"_q, u"gold"_q, u"rose"_q, u"black"_q };
	} else if (style == u"cosmos"_q) {
		return { u"violet"_q, u"blue"_q, u"red"_q, u"teal"_q, u"gold"_q };
	} else if (style == u"frost"_q) {
		return { u"ice"_q, u"snow"_q, u"aurora"_q, u"rose"_q };
	} else if (style == u"neko"_q) {
		return {
			u"pink"_q,
			u"black"_q,
			u"white"_q,
			u"ginger"_q,
			u"lilac"_q,
		};
	} else if (style == u"angel"_q) {
		return { u"gold"_q, u"silver"_q, u"rose"_q, u"sky"_q };
	} else if (style == u"devil"_q) {
		return { u"red"_q, u"purple"_q, u"black"_q, u"green"_q };
	}
	return { QString() };
}

QStringList ToggleKeys(const QString &style) {
	if (style == u"inferno"_q) {
		return { u"embers"_q };
	} else if (style == u"pixel"_q) {
		return { u"stars"_q };
	} else if (style == u"cosmos"_q) {
		return { u"planet"_q, u"meteors"_q };
	} else if (style == u"frost"_q) {
		return { u"snow"_q, u"cap"_q };
	} else if (style == u"neko"_q) {
		return { u"whiskers"_q, u"paw"_q };
	} else if (style == u"angel"_q) {
		return { u"wings"_q, u"sparkle"_q };
	} else if (style == u"devil"_q) {
		return { u"tail"_q, u"pulse"_q };
	}
	return {};
}

std::optional<QStringList> AllowedValues(
		const QString &style,
		const QString &key) {
	if (key.isEmpty()) {
		return std::nullopt;
	} else if (key == QLatin1String(kSpeed)) {
		return Speeds();
	} else if (key == QLatin1String(kAnim)) {
		return OnOff();
	} else if (key == QLatin1String(kGlow)) {
		return std::nullopt;
	} else if (key == PaletteKey(style)) {
		return PaletteValues(style);
	}
	if (style == u"solitaire"_q) {
		if (key == QLatin1String(kGem)) {
			return QStringList{
				u"diamond"_q,
				u"ruby"_q,
				u"emerald"_q,
				u"sapphire"_q,
				u"amethyst"_q,
				u"none"_q,
			};
		} else if (key == QLatin1String(kGemPos)) {
			return QStringList{ u"bottom"_q, u"top"_q, u"right"_q };
		}
	}
	if (ToggleKeys(style).contains(key)) {
		return OnOff();
	}
	return std::nullopt;
}

QString DefaultValue(const QString &style, const QString &key) {
	if (key == QLatin1String(kGlow)) {
		return QString::number(kDefaultGlow);
	} else if (key == QLatin1String(kSpeed)) {
		return u"1"_q;
	}
	const auto allowed = AllowedValues(style, key);
	return (allowed && !allowed->isEmpty()) ? allowed->front() : QString();
}

Values Sanitize(const QString &style, const Values &raw) {
	auto result = Values();
	if (!IsKnownStyle(style)) {
		return result;
	}
	for (auto i = raw.begin(); i != raw.end(); ++i) {
		const auto key = i.key().trimmed();
		const auto value = i.value().trimmed();
		if (key.isEmpty() || value.isEmpty()) {
			continue;
		}
		const auto allowed = AllowedValues(style, key);
		if (allowed && allowed->contains(value)) {
			result.insert(key, value);
		} else if (key == QLatin1String(kGlow)) {
			const auto glow = ParseGlowStrict(value);
			if (glow >= 0) {
				result.insert(key, QString::number(glow));
			}
		}
	}
	return result;
}

Values NonDefault(const QString &style, const Values &values) {
	auto result = Values();
	const auto sanitized = Sanitize(style, values);
	for (auto i = sanitized.begin(); i != sanitized.end(); ++i) {
		if (i.value() != DefaultValue(style, i.key())) {
			result.insert(i.key(), i.value());
		}
	}
	return result;
}

QString ToCommand(const QString &style, const Values &values) {
	if (!IsKnownStyle(style)) {
		return u"frame none"_q;
	}
	auto result = u"frame "_q + style;
	const auto changed = NonDefault(style, values);
	for (auto i = changed.begin(); i != changed.end(); ++i) {
		result += ' ' + i.key() + '=' + i.value();
	}
	return result;
}

} // namespace StaticGram::FrameParams
