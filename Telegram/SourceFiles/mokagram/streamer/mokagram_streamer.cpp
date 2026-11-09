// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 2775002d
// StaticGram: the mokaGram feature behind this API is removed; this is an
// inert stub that keeps the interface so the call sites compile unchanged.
#include "mokagram/streamer/mokagram_streamer.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>

namespace Mokagram::Streamer {

void RegisterSettings() {
}

bool Active(Field) {
	return false;
}

rpl::producer<bool> ActiveValue(Field) {
	return rpl::single(false);
}

rpl::producer<> Changed() {
	return rpl::never<>();
}

QString Mask(Field, const QString &real) {
	return real;
}

TextWithEntities Mask(Field, TextWithEntities real) {
	return real;
}

QString MaskUrl(const QString &url) {
	return url;
}

QString ForCopy(Field, const QString &real) {
	return real;
}

bool CopyBlocked(Field) {
	return false;
}

bool SetClipboard(Field, const QString &real) {
	QGuiApplication::clipboard()->setText(real);
	return true;
}

} // namespace Mokagram::Streamer
