/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace StaticGram {

// The value of a lang key: StaticGram keys get their Russian text when the
// current language is Russian and the language pack does not translate them.
[[nodiscard]] QString LangValue(ushort key, QString value);

} // namespace StaticGram
