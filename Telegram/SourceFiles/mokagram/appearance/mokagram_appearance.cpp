// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 1a4e9630
// Mokagram: оформление, настраиваемое в «Кастомизации».
#include "mokagram/appearance/mokagram_appearance.h"

#include "mokagram/settings/mokagram_settings.h"

// StaticGram: only the built-in look is kept. Custom bubble colors, the
// "select 3000 messages" option and the music player extras are removed.

namespace Mokagram::Appearance {

QColor ColorSetting(const QString &) {
	return QColor();
}

int SelectionLimit() {
	return 100;
}

void RegisterSettings() {
	// Material-3 "expressive" wavy playback slider of the media viewer.
	Settings::Register({
		.key = u"player.expressive_seek"_q,
		.kind = Settings::Kind::Bool,
		.defaultValue = true,
		.hidden = true,
	});
}

void Start() {
}

} // namespace Mokagram::Appearance
