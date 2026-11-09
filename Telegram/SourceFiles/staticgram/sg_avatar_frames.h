/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QMap>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtGui/QImage>

class QPainter;

namespace StaticGram {

// exteraStatic avatar frames (Android AvatarFrames / FrameRenderer, iOS
// StaticGramAvatarFrames.swift): inferno / pixel / solitaire / cosmos /
// frost with their settings (frames-demo/CONTRACT.md).
//
// Everything blurred is pre-rendered per look and pixel size; a frame
// animates at most at 30 fps, only while it is being painted into a widget,
// in at most 6 places at once and only when the caller allows it (power
// saving, disabled animations and anim=0 draw the static first frame).

struct AvatarFrame {
	QString style;
	QMap<QString, QString> params;

	friend inline bool operator==(
		const AvatarFrame &a,
		const AvatarFrame &b) {
		return (a.style == b.style) && (a.params == b.params);
	}
	friend inline bool operator!=(
		const AvatarFrame &a,
		const AvatarFrame &b) {
		return !(a == b);
	}
};

[[nodiscard]] bool IsKnownAvatarFrameStyle(const QString &style);

// The whole frame image (side x side pixels) at loop phase 0..1 of a photo
// of radius AvatarFramePhotoRadius(style) * side / 100 in its center.
[[nodiscard]] QImage RenderAvatarFrame(
	const AvatarFrame &frame,
	int side,
	bool light = false,
	double phase = 0.);
[[nodiscard]] double AvatarFramePhotoRadius(const QString &style);

// Colour dots for the pickers.
[[nodiscard]] QColor AvatarFramePaletteColor(const QString &style, int index);
[[nodiscard]] QColor AvatarFrameGemColor(int index);

// Paints the frame around the photo (a circle inscribed in photo), centered
// on it, shrunk to fit into allowed (not below 86% of its natural size).
// With animate the frame is live while it is painted into a widget.
void PaintAvatarFrame(
	QPainter &p,
	const AvatarFrame &frame,
	QRectF photo,
	QRectF allowed,
	double opacity = 1.,
	bool animate = false,
	bool light = false);

// Drops every pre-rendered frame (theme change).
void ClearAvatarFramesCache();

} // namespace StaticGram
