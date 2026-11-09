/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_avatar_frames.h"

#include "staticgram/sg_frame_params.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QThreadPool>
#include <QtCore/QTimer>
#include <QtCore/QCoreApplication>
#include <QtGui/QLinearGradient>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPainterPathStroker>
#include <QtGui/QRadialGradient>
#include <QtWidgets/QWidget>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace StaticGram {
namespace {

constexpr auto kPi = 3.14159265358979323846;
constexpr auto kTau = kPi * 2.;
constexpr auto kLoop = 3.; // Seconds of one animation loop at speed 1.
constexpr auto kMinFit = 0.86;
constexpr auto kMaxSide = 720;
constexpr auto kInfernoStrip = 16;
constexpr auto kStripMaxSide = 224;
constexpr auto kPixelStates = 24;
constexpr auto kOutline = uint32_t(0x120A1F);
constexpr auto kMaxAnimated = 6;
constexpr auto kFrameInterval = 33;
constexpr auto kSlotIdle = qint64(250);
constexpr auto kSlotRetry = 1000;
constexpr auto kCacheBytes = qint64(32 * 1024 * 1024);

// The reference renders show about 60% of the nominal CSS blur
// (Android FrameRenderer.BLUR_K): a CSS blur(px) is a sigma of 0.6 px.
constexpr auto kBlurK = 0.6;

constexpr auto kSparkleR = 20.;
constexpr auto kSparkleSide = 72;
constexpr auto kFlakeR = 16.;
constexpr auto kFlakeSide = 48;

using Draw = std::function<void(QPainter&)>;

class Rng final {
public:
	explicit Rng(uint32_t seed) : _s(seed) {
	}
	double next() {
		_s = _s * 1664525U + 1013904223U;
		return double(_s) / 4294967296.;
	}

private:
	uint32_t _s = 0;

};

using Pal = std::vector<uint32_t>;

const std::vector<Pal> kInfernoPal = {
	{ 0xff2800, 0xc81400, 0x780000, 0xff7800, 0xff5000, 0xff3c00, 0xfff096, 0xffbe28, 0xff8c00, 0xff6e00, 0xffd36b, 0xfff3c4, 0xffb347, 0xfff1a8, 0xff7b00 },
	{ 0x0050ff, 0x0028c8, 0x000a78, 0x0096ff, 0x006eff, 0x0050ff, 0xd2f5ff, 0x5ac8ff, 0x008cff, 0x0078ff, 0x8fd0ff, 0xe6f6ff, 0x7fc8ff, 0xe8f8ff, 0x2a7bff },
	{ 0x28c800, 0x148c00, 0x005000, 0x78ff28, 0x50dc14, 0x28b400, 0xe6ffaa, 0xaaff50, 0x64dc00, 0x5ae600, 0xc8ff7a, 0xf2ffd8, 0xb4ff6a, 0xefffc8, 0x4cd600 },
	{ 0x9600ff, 0x6400c8, 0x3c0078, 0xbe50ff, 0xa032ff, 0x821ee6, 0xf5d2ff, 0xdc96ff, 0xb45aff, 0xaa3cff, 0xe0b0ff, 0xf8eaff, 0xd39bff, 0xf6e6ff, 0x9b30ff },
	{ 0xff0078, 0xc8005a, 0x78003c, 0xff5aaa, 0xff3c96, 0xff2882, 0xffe1f0, 0xffa0d2, 0xff64b4, 0xff46a0, 0xffb3dc, 0xfff0f8, 0xff9fd2, 0xffe6f3, 0xff3fa0 },
};
const std::vector<Pal> kPixelPal = {
	{ 0xff004d, 0xff77a8, 0xffa300, 0xffec27, 0x00e436, 0x29adff, 0x83769c, 0xffec27, 0xff77a8 },
	{ 0xffb3c7, 0xffd6a5, 0xfdffb6, 0xcaffbf, 0x9bf6ff, 0xa0c4ff, 0xbdb2ff, 0xfdffb6, 0xffb3c7 },
	{ 0xff00ff, 0x00ffff, 0x39ff14, 0xffff00, 0xff3131, 0xbc13fe, 0x00b3ff, 0x39ff14, 0xff00ff },
	{ 0xffffff, 0xd9d9d9, 0xb3b3b3, 0x8c8c8c, 0x666666, 0x8c8c8c, 0xd9d9d9, 0xffffff, 0xb3b3b3 },
	{ 0xfff1a8, 0xffd84a, 0xf4b400, 0xc98a00, 0x8a5a00, 0xc98a00, 0xffd84a, 0xffd84a, 0xfff1a8 },
};
const std::vector<Pal> kMetalPal = {
	{ 0x7d8189, 0xffffff, 0xa3a7ae, 0xeef0f3, 0x6f737b, 0xffffff, 0x959aa2, 0xf6f7f9, 0x7d8189, 0xf4f5f7, 0xd9dde3, 0xffffff, 0x7b7f86 },
	{ 0x8a6a1f, 0xfff3b0, 0xb8892b, 0xf7e08a, 0x7a5a14, 0xfff3b0, 0xa67c22, 0xfbe9a6, 0x8a6a1f, 0xfff1c2, 0xe8cf86, 0xfff6d0, 0x8a6a1f },
	{ 0x8a4f4a, 0xffe1d6, 0xb9786d, 0xf6c9bb, 0x7a4440, 0xffe1d6, 0xa86a60, 0xfad6ca, 0x8a4f4a, 0xffe4da, 0xeec0b3, 0xfff0ea, 0x8a4f4a },
	{ 0x111113, 0x6b6d73, 0x1f2023, 0x4a4c52, 0x0b0b0c, 0x7a7c82, 0x1a1b1e, 0x505258, 0x111113, 0x9a9ca3, 0x3a3b40, 0x8a8c92, 0x0d0d0f },
};
const std::vector<Pal> kGemPal = {
	{ 0x5a6273, 0xffffff, 0xffffff, 0xe8f0ff, 0xffe3f3, 0xffffff, 0xfff4d6, 0xe6ecf5, 0xffffff, 0xf3e6ff, 0xffffff, 0xc9d0da, 0xe8f0ff },
	{ 0x5a0414, 0xff6f8f, 0xffd0dc, 0xff9fb5, 0xffe6ec, 0xff8aa5, 0xffd0dc, 0xffb3c4, 0xffe6ec, 0xff9fb5, 0xffb3c4, 0xc0143c, 0xffc0cf },
	{ 0x003d22, 0x5cf0a0, 0xc8ffe0, 0x8af5be, 0xe6fff1, 0x6fe8a8, 0xc8ffe0, 0xa8f5cf, 0xe6fff1, 0x8af5be, 0xa8f5cf, 0x0f8a50, 0xc0ffe0 },
	{ 0x0a1a66, 0x6fa8ff, 0xcfe0ff, 0x9fc0ff, 0xe6efff, 0x7fa8ff, 0xcfe0ff, 0xb0ccff, 0xe6efff, 0x9fc0ff, 0xb0ccff, 0x1f4fc0, 0xc8dcff },
	{ 0x2e0e5a, 0xc89bff, 0xeedcff, 0xd6b3ff, 0xf6ecff, 0xc39bff, 0xeedcff, 0xdcc0ff, 0xf6ecff, 0xd6b3ff, 0xdcc0ff, 0x6a2fc0, 0xe6d0ff },
};
const std::vector<Pal> kCosmosPal = {
	{ 0x2a0b52, 0x8f2bd1, 0xff4fa3, 0x3b0f73, 0xffc8f0, 0xffb3f0, 0xffe6c7, 0xff8fb1, 0x6a1b9a, 0xffd6aa, 0xff9ad5 },
	{ 0x071a4a, 0x1f5fd1, 0x4fd1ff, 0x0d2a73, 0xbee1ff, 0x9fdcff, 0xe6f4ff, 0x6fb7ff, 0x1a3a9a, 0xc8e6ff, 0x7fc4ff },
	{ 0x4a0710, 0xc21f2f, 0xff8a4f, 0x6b0f1a, 0xffd2be, 0xffb39a, 0xfff0d0, 0xff8a5f, 0x8a1a1a, 0xffdcb4, 0xff7a5a },
	{ 0x052e2e, 0x118a7e, 0x4fffd6, 0x0b4a4a, 0xbefff0, 0x9ffff0, 0xe8fff9, 0x5fe0c8, 0x0e5a5a, 0xc8fff0, 0x5fffe0 },
	{ 0x3a2306, 0xb07a12, 0xffd36b, 0x5a3a0a, 0xffecbe, 0xffe3a0, 0xfff6dc, 0xffc860, 0x8a5a10, 0xffe6b4, 0xffcf70 },
};
const std::vector<Pal> kFrostPal = {
	{ 0xffffff, 0xbfe3ff, 0xf2f9ff, 0x9fd0ff, 0xffffff, 0xcbe8ff, 0xffffff, 0xa0d2ff, 0xd2ebff, 0xe1f0ff, 0x285a96, 0xd7e9f8, 0xeef6fd, 0xebf6ff, 0xbee1ff, 0x4678af, 0x3c78be, 0x3a6e9e },
	{ 0xffffff, 0xe6e9ee, 0xffffff, 0xd5dae2, 0xffffff, 0xeceff3, 0xffffff, 0xdce1eb, 0xf0f2f8, 0xf5f6fa, 0x505a6e, 0xe3e6ec, 0xf6f7fa, 0xf5f6fa, 0xd8dce4, 0x6e788c, 0x788296, 0x5a6478 },
	{ 0xb8ffe9, 0x6ff7c8, 0x9fd8ff, 0xc7a8ff, 0xb8ffe9, 0x7fe0ff, 0xb8ffe9, 0x78ffd2, 0xaaffe1, 0xd7fff5, 0x1e6e64, 0xd2f5ec, 0xeefcf8, 0xdcfff4, 0xa0f0dc, 0x3c8c8c, 0x50c8b4, 0x2e7a72 },
	{ 0xffffff, 0xffd1e6, 0xfff2f8, 0xffb3d6, 0xffffff, 0xffd9ea, 0xffffff, 0xffb4d7, 0xffdceb, 0xffebf5, 0x8c3c6e, 0xf6dbe8, 0xfdf0f6, 0xfff0f7, 0xffc8e1, 0xaf5a82, 0xc85a96, 0x9a4a72 },
};

const Pal &PickPal(const std::vector<Pal> &list, int index) {
	return list[std::clamp(index, 0, int(list.size()) - 1)];
}

QColor Color(uint32_t rgb, double alpha = 1.) {
	return QColor(
		int((rgb >> 16) & 0xFF),
		int((rgb >> 8) & 0xFF),
		int(rgb & 0xFF),
		int(std::round(std::clamp(alpha, 0., 1.) * 255.)));
}

uint32_t LerpRgb(uint32_t a, uint32_t b, double t) {
	const auto channel = [&](int shift) {
		const auto ca = double((a >> shift) & 0xFF);
		const auto cb = double((b >> shift) & 0xFF);
		const auto v = std::clamp(std::round(ca + (cb - ca) * t), 0., 255.);
		return uint32_t(v);
	};
	return (channel(16) << 16) | (channel(8) << 8) | channel(0);
}

uint32_t Shade(uint32_t rgb) {
	const auto k = 0.62;
	const auto r = uint32_t(double((rgb >> 16) & 0xFF) * k);
	const auto g = uint32_t(double((rgb >> 8) & 0xFF) * k);
	const auto b = uint32_t(double(rgb & 0xFF) * k);
	return (r << 16) | (g << 8) | b;
}

uint32_t LerpStops(const Pal &colors, double t) {
	if (colors.size() == 1) {
		return colors[0];
	}
	const auto position = std::clamp(t, 0., 1.) * double(colors.size() - 1);
	const auto i = std::min(int(colors.size()) - 2, int(position));
	return LerpRgb(colors[i], colors[i + 1], position - i);
}

QRectF Circle(QPointF c, double r) {
	return QRectF(c.x() - r, c.y() - r, r * 2., r * 2.);
}

QImage MakeLayer(int side) {
	auto result = QImage(side, side, QImage::Format_ARGB32_Premultiplied);
	result.fill(Qt::transparent);
	return result;
}

void BoxBlurPass(QImage &image, int radius, bool horizontal) {
	const auto w = image.width();
	const auto h = image.height();
	const auto lines = horizontal ? h : w;
	const auto length = horizontal ? w : h;
	const auto stride = image.bytesPerLine() / 4;
	const auto step = horizontal ? 1 : stride;
	auto bits = reinterpret_cast<uint32_t*>(image.bits());
	auto line = std::vector<uint32_t>(length + 2 * radius + 1, 0);
	const auto window = uint32_t(2 * radius + 1);
	for (auto l = 0; l != lines; ++l) {
		const auto base = bits + (horizontal ? (l * stride) : l);
		for (auto i = 0; i != length; ++i) {
			line[radius + i] = base[i * step];
		}
		auto s0 = uint32_t(0);
		auto s1 = uint32_t(0);
		auto s2 = uint32_t(0);
		auto s3 = uint32_t(0);
		for (auto i = 0; i != 2 * radius; ++i) {
			const auto v = line[i];
			s0 += (v & 0xFF);
			s1 += ((v >> 8) & 0xFF);
			s2 += ((v >> 16) & 0xFF);
			s3 += (v >> 24);
		}
		for (auto i = 0; i != length; ++i) {
			const auto in = line[i + 2 * radius];
			s0 += (in & 0xFF);
			s1 += ((in >> 8) & 0xFF);
			s2 += ((in >> 16) & 0xFF);
			s3 += (in >> 24);
			base[i * step] = (s0 / window)
				| ((s1 / window) << 8)
				| ((s2 / window) << 16)
				| ((s3 / window) << 24);
			const auto out = line[i];
			s0 -= (out & 0xFF);
			s1 -= ((out >> 8) & 0xFF);
			s2 -= ((out >> 16) & 0xFF);
			s3 -= (out >> 24);
		}
	}
}

// Gaussian blur approximation (three box passes) with the given sigma.
void Blur(QImage &image, double sigma) {
	if (sigma < 0.3) {
		return;
	}
	const auto radius = std::max(
		1,
		int(std::round((std::sqrt(4. * sigma * sigma + 1.) - 1.) / 2.)));
	for (auto pass = 0; pass != 3; ++pass) {
		BoxBlurPass(image, radius, true);
		BoxBlurPass(image, radius, false);
	}
}

void PrepareHq(QPainter &p) {
	p.setRenderHint(QPainter::Antialiasing);
	p.setRenderHint(QPainter::SmoothPixmapTransform);
}

// Draws into a separate layer, blurs it (CSS filter: blur(sigma)) and
// composites it with the given mode and opacity.
void DrawBlurred(
		QPainter &p,
		int side,
		double sigma,
		QPainter::CompositionMode mode,
		double alpha,
		const Draw &draw) {
	auto layer = MakeLayer(side);
	{
		auto q = QPainter(&layer);
		PrepareHq(q);
		draw(q);
	}
	Blur(layer, sigma);
	p.save();
	p.setCompositionMode(mode);
	p.setOpacity(p.opacity() * std::clamp(alpha, 0., 1.));
	p.drawImage(0, 0, layer);
	p.restore();
}

QRect LayerRect(QRectF bounds, double margin, int side) {
	return bounds.adjusted(
		-margin,
		-margin,
		margin,
		margin).toAlignedRect().intersected(QRect(0, 0, side, side));
}

// Canvas / CoreGraphics like shadow: the shape drawn by draw() gets a
// blurred copy of itself in color under it (blur = shadowBlur).
void DrawWithShadow(
		QPainter &p,
		QRectF bounds,
		int side,
		double blur,
		QPointF offset,
		QColor color,
		double alpha,
		const Draw &draw) {
	const auto rect = LayerRect(
		bounds,
		1.5 * blur + std::abs(offset.x()) + std::abs(offset.y()) + 2.,
		side);
	if (rect.isEmpty()) {
		return;
	}
	auto layer = QImage(rect.size(), QImage::Format_ARGB32_Premultiplied);
	layer.fill(Qt::transparent);
	{
		auto q = QPainter(&layer);
		PrepareHq(q);
		q.translate(-rect.topLeft());
		draw(q);
	}
	auto shadow = layer;
	{
		auto q = QPainter(&shadow);
		q.setCompositionMode(QPainter::CompositionMode_SourceIn);
		q.fillRect(shadow.rect(), color);
	}
	Blur(shadow, blur / 2.);
	p.save();
	p.setOpacity(p.opacity() * std::clamp(alpha, 0., 1.));
	p.drawImage(QPointF(rect.topLeft()) + offset, shadow);
	p.drawImage(rect.topLeft(), layer);
	p.restore();
}

// Conic gradient (canvas createConicGradient(rot, c, c)) as an image.
QImage ConicImage(
		QRect rect,
		QPointF center,
		double rot,
		const Pal &colors,
		double alpha) {
	auto result = QImage(rect.size(), QImage::Format_ARGB32_Premultiplied);
	const auto a = int(std::round(std::clamp(alpha, 0., 1.) * 255.));
	for (auto y = 0; y != rect.height(); ++y) {
		auto line = reinterpret_cast<uint32_t*>(result.scanLine(y));
		const auto dy = rect.y() + y + 0.5 - center.y();
		for (auto x = 0; x != rect.width(); ++x) {
			const auto dx = rect.x() + x + 0.5 - center.x();
			auto t = (std::atan2(dy, dx) - rot) / kTau;
			t -= std::floor(t);
			const auto rgb = LerpStops(colors, t);
			const auto r = ((rgb >> 16) & 0xFF) * a / 255;
			const auto g = ((rgb >> 8) & 0xFF) * a / 255;
			const auto b = (rgb & 0xFF) * a / 255;
			line[x] = (uint32_t(a) << 24)
				| (uint32_t(r) << 16)
				| (uint32_t(g) << 8)
				| uint32_t(b);
		}
	}
	return result;
}

QPainterPath StrokedPath(
		const QPainterPath &path,
		double width,
		Qt::PenCapStyle cap) {
	auto stroker = QPainterPathStroker();
	stroker.setWidth(width);
	stroker.setCapStyle(cap);
	stroker.setJoinStyle(Qt::RoundJoin);
	return stroker.createStroke(path);
}

void StrokeConic(
		QPainter &p,
		const QPainterPath &path,
		double width,
		Qt::PenCapStyle cap,
		QPointF center,
		double rot,
		const Pal &colors,
		double alpha = 1.) {
	const auto stroke = StrokedPath(path, width, cap);
	const auto rect = stroke.boundingRect().toAlignedRect();
	if (rect.isEmpty()) {
		return;
	}
	auto brush = QBrush(ConicImage(rect, center, rot, colors, alpha));
	brush.setTransform(QTransform::fromTranslate(rect.x(), rect.y()));
	p.save();
	p.setPen(Qt::NoPen);
	p.setBrush(brush);
	p.drawPath(stroke);
	p.restore();
}

QPainterPath CirclePath(QPointF c, double r) {
	auto result = QPainterPath();
	result.addEllipse(Circle(c, r));
	return result;
}

// Arc from a0 to a1 (radians, a1 > a0, y down like canvas).
QPainterPath ArcPath(QPointF c, double r, double a0, double a1) {
	auto result = QPainterPath();
	const auto steps = std::max(8, int(std::ceil((a1 - a0) / kTau * 180.)));
	for (auto i = 0; i <= steps; ++i) {
		const auto a = a0 + (a1 - a0) * i / steps;
		const auto point = QPointF(
			c.x() + r * std::cos(a),
			c.y() + r * std::sin(a));
		if (!i) {
			result.moveTo(point);
		} else {
			result.lineTo(point);
		}
	}
	return result;
}

void StrokeCircle(QPainter &p, QPointF c, double r, QColor color, double w) {
	p.save();
	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(color, w));
	p.drawEllipse(Circle(c, r));
	p.restore();
}

void FillCircle(QPainter &p, QPointF c, double r, QColor color) {
	p.save();
	p.setPen(Qt::NoPen);
	p.setBrush(color);
	p.drawEllipse(Circle(c, r));
	p.restore();
}

QPainterPath Star4Path(double x, double y, double r, double rot) {
	const auto k = r * 0.16;
	auto path = QPainterPath(QPointF(0., -r));
	path.quadTo(QPointF(k, -k), QPointF(r, 0.));
	path.quadTo(QPointF(k, k), QPointF(0., r));
	path.quadTo(QPointF(-k, k), QPointF(-r, 0.));
	path.quadTo(QPointF(-k, -k), QPointF(0., -r));
	path.closeSubpath();
	auto transform = QTransform();
	transform.translate(x, y);
	transform.rotateRadians(rot);
	return transform.map(path);
}

// Removes everything under the photo.
void ClearPhoto(QPainter &p, QPointF c, double r) {
	p.save();
	p.setCompositionMode(QPainter::CompositionMode_Clear);
	p.setPen(Qt::NoPen);
	p.setBrush(Qt::black);
	p.drawEllipse(Circle(c, r));
	p.restore();
}

// Android radial(): stops are given between r0 and r1, the gradient ends
// at r1 and clamps inside r0.
QRadialGradient RadialFrom(
		QPointF center,
		double r0,
		double r1,
		const std::vector<std::pair<double, QColor>> &stops) {
	auto result = QRadialGradient(center, std::max(r1, 0.01));
	for (const auto &[position, color] : stops) {
		const auto at = (r1 > 0.) ? ((r0 + position * (r1 - r0)) / r1) : 0.;
		result.setColorAt(std::clamp(at, 0., 1.), color);
	}
	return result;
}

void DrawRotated(
		QPainter &p,
		const QImage &image,
		double c,
		double rotation,
		double opacity = 1.,
		QPainter::CompositionMode mode = QPainter::CompositionMode_SourceOver) {
	if (image.isNull() || opacity <= 0.) {
		return;
	}
	p.save();
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	p.setCompositionMode(mode);
	p.setOpacity(p.opacity() * std::min(opacity, 1.));
	p.translate(c, c);
	p.rotate(rotation * 180. / kPi);
	p.translate(-c, -c);
	p.drawImage(0, 0, image);
	p.restore();
}

// ------------------------------------------------------------- look

struct Look {
	QString style;
	int palette = 0;
	int gem = 0;
	int gemPos = 0;
	bool toggle1 = true;
	bool toggle2 = true;
	double speed = 1.;
	bool anim = true;
	int glow = FrameParams::kDefaultGlow;
	double g = 1.;
	QString key; // Everything that changes the look (not speed or anim).
};

[[nodiscard]] int IndexIn(const QStringList &values, const QString &value) {
	return std::max(0, int(values.indexOf(value.trimmed())));
}

[[nodiscard]] Look ComputeLook(const AvatarFrame &frame) {
	const auto &style = frame.style;
	const auto values = FrameParams::Sanitize(style, frame.params);
	const auto value = [&](const QString &key) {
		return values.value(key);
	};
	auto result = Look();
	result.style = style;
	result.palette = IndexIn(
		FrameParams::PaletteValues(style),
		value(FrameParams::PaletteKey(style)));
	if (style == u"solitaire"_q) {
		const auto gems = FrameParams::AllowedValues(style, u"gem"_q);
		const auto positions = FrameParams::AllowedValues(style, u"gempos"_q);
		result.gem = gems ? IndexIn(*gems, value(u"gem"_q)) : 0;
		result.gemPos = positions
			? IndexIn(*positions, value(u"gempos"_q))
			: 0;
	}
	const auto toggles = FrameParams::ToggleKeys(style);
	result.toggle1 = (toggles.size() < 1) || (value(toggles[0]) != u"0"_q);
	result.toggle2 = (toggles.size() < 2) || (value(toggles[1]) != u"0"_q);
	const auto speed = value(u"speed"_q);
	result.speed = FrameParams::Speeds().contains(speed)
		? speed.toDouble()
		: 1.;
	result.anim = (value(u"anim"_q) != u"0"_q);
	auto ok = false;
	const auto glow = value(u"glow"_q).toInt(&ok);
	result.glow = (ok && glow >= 0 && glow <= 100)
		? glow
		: FrameParams::kDefaultGlow;
	result.g = result.glow / double(FrameParams::kDefaultGlow);
	result.key = style
		+ QString::fromLatin1("|%1|%2|%3|%4|%5|%6").arg(
			QString::number(result.palette),
			QString::number(result.gem),
			QString::number(result.gemPos),
			QString::number(result.toggle1 ? 1 : 0),
			QString::number(result.toggle2 ? 1 : 0),
			QString::number(result.glow));
	return result;
}

[[nodiscard]] double Phase(qint64 ms, double speed) {
	const auto t = ms / 1000. * speed;
	return std::fmod(t, kLoop) / kLoop;
}

[[nodiscard]] qint64 Now() {
	static const auto timer = [] {
		auto result = QElapsedTimer();
		result.start();
		return result;
	}();
	return timer.elapsed();
}

// ------------------------------------------------------------- layers

// Everything pre-rendered for one look (and light / dark) at one size.
struct Layers {
	Layers(Look data, bool isLight, int size)
	: look(std::move(data))
	, light(isLight)
	, side(size)
	, u(size / 100.)
	, c(size / 2.)
	, g(look.g)
	, stripSide((look.style == u"inferno"_q)
		? std::min(size, kStripMaxSide)
		: 0) {
		if (stripSide) {
			strip.resize(kInfernoStrip);
		}
	}

	[[nodiscard]] qint64 bytes() const {
		const auto full = qint64(side) * side * 4;
		const auto count = (look.style == u"pixel"_q)
			? 1
			: (look.style == u"inferno"_q)
			? 2
			: (look.style == u"solitaire"_q)
			? 4
			: 4;
		const auto stripBytes = stripSide
			? qint64(kInfernoStrip) * stripSide * stripSide * 4
			: 0;
		return full * (count + 1) + stripBytes + 64 * 1024;
	}

	const Look look;
	const bool light = false;
	const int side = 0;
	const double u = 1.;
	const double c = 0.;
	const double g = 1.;

	bool built = false;
	QImage poster;
	QImage under;
	QImage over;
	QImage rot1;
	QImage rot2;
	QImage sparkle;
	QImage glowDot;
	QImage planet;
	QImage flake;
	QImage metal;
	std::vector<QImage> pixels;
	std::vector<QPainterPath> facets;
	QPainterPath metalLines;
	double jx = 0.;
	double jy = 0.;

	const int stripSide = 0;
	std::vector<QImage> strip;
	std::atomic<bool> stripReady { false };
	bool stripRequested = false;
};

[[nodiscard]] QImage SparkleImage(uint32_t glow) {
	auto result = MakeLayer(kSparkleSide);
	auto p = QPainter(&result);
	PrepareHq(p);
	const auto c = kSparkleSide / 2.;
	DrawWithShadow(
		p,
		Circle({ c, c }, kSparkleR),
		kSparkleSide,
		kSparkleR * 1.6,
		QPointF(),
		Color(glow),
		1.,
		[&](QPainter &q) {
			q.setPen(Qt::NoPen);
			q.setBrush(Color(0xFFFFFF));
			q.drawPath(Star4Path(c, c, kSparkleR, 0.));
			q.setBrush(Color(0xFFFFFF, 0.85));
			q.drawPath(Star4Path(c, c, kSparkleR * 0.55, kPi / 4.));
		});
	p.end();
	return result;
}

void DrawSparkle(
		QPainter &p,
		const QImage &image,
		double x,
		double y,
		double r,
		double rotation,
		double alpha) {
	if (alpha <= 0.01 || r <= 0. || image.isNull()) {
		return;
	}
	p.save();
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	p.setOpacity(p.opacity() * std::min(1., alpha));
	p.translate(x, y);
	p.rotate(rotation * 180. / kPi);
	p.scale(r / kSparkleR, r / kSparkleR);
	p.drawImage(QPointF(-kSparkleSide / 2., -kSparkleSide / 2.), image);
	p.restore();
}

// ------------------------------------------------------------- inferno

struct Embers {
	std::array<double, 16> th0 = { { 0. } };
	std::array<double, 16> off = { { 0. } };
};

[[nodiscard]] const Embers &InfernoEmbers() {
	static const auto result = [] {
		auto embers = Embers();
		auto r = Rng(11);
		for (auto i = 0; i != 16; ++i) {
			embers.th0[i] = r.next() * kTau;
			embers.off[i] = r.next();
		}
		return embers;
	}();
	return result;
}

[[nodiscard]] double FlameF(double th, double k, double w) {
	const auto s = 0.42 * std::sin(9. * th - 2. * w + k)
		+ 0.33 * std::sin(14. * th + 3. * w + 2. * k)
		+ 0.25 * std::sin(23. * th - 5. * w + 1. + k);
	const auto up = 1. + 0.55 * std::max(0., -std::sin(th));
	return std::pow(std::max(0., s + 0.45) / 1.45, 1.6) * up;
}

// The three additive flame layers (blurred), the photo cut out.
void InfernoFlames(
		QPainter &p,
		int side,
		int palette,
		double g,
		double phase) {
	const auto u = side / 100.;
	const auto c = QPointF(side / 2., side / 2.);
	const auto a = 36. * u;
	const auto w = kTau * phase;
	const auto &P = PickPal(kInfernoPal, palette);
	const auto layer = [&](
			double base,
			double amp,
			double k,
			std::vector<std::pair<double, QColor>> stops,
			double blur) {
		// The flames of the reference are crisper: half of the blur.
		DrawBlurred(
			p,
			side,
			blur * 0.5 * kBlurK,
			QPainter::CompositionMode_Plus,
			1.,
			[&](QPainter &q) {
				auto path = QPainterPath();
				path.setFillRule(Qt::OddEvenFill);
				for (auto i = 0; i <= 240; ++i) {
					const auto th = i / 240. * kTau;
					const auto r = a + base + amp * FlameF(th, k, w);
					const auto point = QPointF(
						c.x() + r * std::cos(th),
						c.y() + r * std::sin(th));
					if (!i) {
						path.moveTo(point);
					} else {
						path.lineTo(point);
					}
				}
				path.closeSubpath();
				path.addEllipse(Circle(c, a - 2. * u));
				q.setPen(Qt::NoPen);
				q.setBrush(RadialFrom(
					c,
					a - 2. * u,
					a + base + amp * 1.5,
					stops));
				q.drawPath(path);
			});
	};
	layer(4. * u, 12. * u, 0., {
		{ 0., Color(P[0], 0.9 * g) },
		{ 0.55, Color(P[1], 0.55 * g) },
		{ 1., Color(P[2], 0.) },
	}, 2.2 * u);
	layer(3. * u, 9. * u, 1.7, {
		{ 0., Color(P[3], 0.95) },
		{ 0.5, Color(P[4], 0.5) },
		{ 1., Color(P[5], 0.) },
	}, 1.2 * u);
	layer(1.5 * u, 5. * u, 3.1, {
		{ 0., Color(P[6], 1.) },
		{ 0.45, Color(P[7], 0.7) },
		{ 1., Color(P[8], 0.) },
	}, 0.6 * u);
	ClearPhoto(p, c, a);
}

void BuildInferno(Layers &L) {
	const auto u = L.u;
	const auto c = QPointF(L.c, L.c);
	const auto a = 36. * u;
	const auto g = L.g;
	const auto &P = PickPal(kInfernoPal, L.look.palette);
	L.over = MakeLayer(L.side);
	auto p = QPainter(&L.over);
	PrepareHq(p);
	// Heat glow over the photo edge.
	p.setPen(Qt::NoPen);
	p.setBrush(RadialFrom(c, a * 0.7, a, {
		{ 0., Color(P[9], 0.) },
		{ 1., Color(P[9], 0.5 * g) },
	}));
	p.drawEllipse(Circle(c, a));
	DrawBlurred(
		p,
		L.side,
		0.8 * u * kBlurK,
		QPainter::CompositionMode_SourceOver,
		1.,
		[&](QPainter &q) { StrokeCircle(q, c, a, Color(P[10]), 2.2 * u); });
	StrokeCircle(p, c, a + 0.3 * u, Color(P[11]), 0.8 * u);
	p.end();

	// A soft dot for the ember glow.
	L.glowDot = MakeLayer(32);
	auto q = QPainter(&L.glowDot);
	PrepareHq(q);
	auto radial = QRadialGradient(QPointF(16., 16.), 16.);
	radial.setColorAt(0., Color(P[14], 0.9));
	radial.setColorAt(0.45, Color(P[14], 0.35));
	radial.setColorAt(1., Color(P[14], 0.));
	q.setPen(Qt::NoPen);
	q.setBrush(radial);
	q.drawEllipse(QRectF(0., 0., 32., 32.));
	q.end();
}

void BuildInfernoStrip(Layers &L) {
	for (auto i = 0; i != kInfernoStrip; ++i) {
		auto image = MakeLayer(L.stripSide);
		auto p = QPainter(&image);
		PrepareHq(p);
		InfernoFlames(
			p,
			L.stripSide,
			L.look.palette,
			L.g,
			i / double(kInfernoStrip));
		p.end();
		L.strip[i] = std::move(image);
	}
	L.stripReady.store(true, std::memory_order_release);
}

void DrawInferno(QPainter &p, Layers &L, double phase, bool useStrip) {
	const auto u = L.u;
	const auto c = L.c;
	const auto a = 36. * u;
	const auto full = QRectF(0., 0., L.side, L.side);
	if (useStrip
		&& L.stripSide
		&& L.stripReady.load(std::memory_order_acquire)) {
		const auto f = phase * kInfernoStrip;
		const auto i0 = int(std::floor(f)) % kInfernoStrip;
		const auto i1 = (i0 + 1) % kInfernoStrip;
		const auto mix = f - std::floor(f);
		// Cross-fade the two nearest flame frames: additive on dark
		// backgrounds, plain alpha on light ones.
		p.save();
		p.setRenderHint(QPainter::SmoothPixmapTransform);
		p.setCompositionMode(L.light
			? QPainter::CompositionMode_SourceOver
			: QPainter::CompositionMode_Plus);
		p.setOpacity(1. - mix);
		p.drawImage(full, L.strip[i0]);
		p.setOpacity(mix);
		p.drawImage(full, L.strip[i1]);
		p.restore();
	}
	p.drawImage(0, 0, L.over);
	if (!L.look.toggle1) {
		return;
	}
	const auto &P = PickPal(kInfernoPal, L.look.palette);
	const auto &embers = InfernoEmbers();
	for (auto i = 0; i != 16; ++i) {
		const auto q = std::fmod(
			phase * (1 + (i % 2)) + embers.off[i],
			1.);
		const auto th = embers.th0[i] + 0.25 * std::sin(kTau * q);
		const auto d = a + (3. + q * 13.) * u;
		const auto alpha = std::sin(kPi * q);
		if (alpha <= 0.01) {
			continue;
		}
		const auto x = c + d * std::cos(th);
		const auto y = c + d * std::sin(th) - q * 4. * u;
		const auto r = (0.6 + 0.6 * (1. - q)) * u;
		const auto gr = r + 2. * u;
		p.save();
		p.setRenderHint(QPainter::SmoothPixmapTransform);
		p.setOpacity(p.opacity() * alpha * std::min(1., L.g));
		p.drawImage(QRectF(x - gr, y - gr, gr * 2., gr * 2.), L.glowDot);
		p.restore();
		FillCircle(
			p,
			QPointF(x, y),
			r,
			Color((i % 3) ? P[12] : P[13], alpha));
	}
}

// ------------------------------------------------------------- pixel

[[nodiscard]] QImage PixelState(const Layers &L, int idx) {
	constexpr auto n = 30;
	const auto cc = n / 2.;
	const auto ra = 12.4;
	const auto &P = PickPal(kPixelPal, L.look.palette);
	const auto g = L.g;
	auto pal = std::array<uint32_t, 7>();
	for (auto i = 0; i != 7; ++i) {
		const auto col = P[i];
		pal[i] = (g < 1.)
			? LerpRgb(col, Shade(col), (1. - g) * 0.6)
			: LerpRgb(col, 0xFFFFFF, (g - 1.) * 0.45);
	}
	const auto step = idx / 2;
	auto px = std::vector<uint32_t>(n * n, 0);
	const auto opaque = [](uint32_t rgb) {
		return 0xFF000000U | (rgb & 0xFFFFFFU);
	};
	for (auto y = 0; y != n; ++y) {
		for (auto x = 0; x != n; ++x) {
			const auto dx = x + 0.5 - cc;
			const auto dy = y + 0.5 - cc;
			const auto d = std::sqrt(dx * dx + dy * dy);
			auto col = uint32_t(0);
			if (d >= ra - 1.6 && d < ra + 1.1) {
				auto rgb = pal[((((x + y) / 3 - step) % 7) + 7) % 7];
				if (d < ra - 0.6) {
					rgb = Shade(rgb);
				}
				col = opaque(rgb);
			} else if ((d >= ra + 1.1 && d < ra + 2.05)
				|| (d >= ra - 2.5 && d < ra - 1.6)) {
				col = opaque(kOutline);
			}
			px[y * n + x] = col;
		}
	}
	if (L.look.toggle1) {
		struct Star {
			int x = 0;
			int y = 0;
			int o = 0;
		};
		const auto stars = std::array<Star, 4>{ {
			{ 3, 4, 0 },
			{ 26, 3, 6 },
			{ 27, 26, 12 },
			{ 3, 26, 18 },
		} };
		const auto cross = std::array<QPoint, 5>{ {
			{ 0, 0 }, { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
		} };
		const auto tips = std::array<QPoint, 5>{ {
			{ 0, 0 }, { 2, 0 }, { -2, 0 }, { 0, 2 }, { 0, -2 },
		} };
		auto star = std::vector<bool>(n * n, false);
		for (const auto &s : stars) {
			const auto on = ((idx + s.o) % 24) < 12;
			const auto big = ((2 * idx + s.o) % 24) < 12;
			const auto col = opaque(on ? P[7] : P[8]);
			for (const auto &q : cross) {
				const auto at = (s.y + q.y()) * n + s.x + q.x();
				px[at] = col;
				star[at] = true;
			}
			if (big) {
				for (const auto &q : tips) {
					const auto at = (s.y + q.y()) * n + s.x + q.x();
					px[at] = (!q.x() && !q.y())
						? opaque(LerpRgb(col, 0xFFFFFF, 0.9))
						: 0xE6FFFFFFU;
					star[at] = true;
				}
			}
		}
		if (L.light) {
			// Light theme: a dark outline so the stars read on white.
			const auto copy = px;
			for (auto y = 0; y != n; ++y) {
				for (auto x = 0; x != n; ++x) {
					if (copy[y * n + x]) {
						continue;
					}
					const auto touching = (x > 0 && star[y * n + x - 1])
						|| (x < n - 1 && star[y * n + x + 1])
						|| (y > 0 && star[(y - 1) * n + x])
						|| (y < n - 1 && star[(y + 1) * n + x]);
					if (touching) {
						px[y * n + x] = opaque(kOutline);
					}
				}
			}
		}
	}
	auto result = QImage(n, n, QImage::Format_ARGB32);
	for (auto y = 0; y != n; ++y) {
		auto line = reinterpret_cast<uint32_t*>(result.scanLine(y));
		for (auto x = 0; x != n; ++x) {
			line[x] = px[y * n + x];
		}
	}
	return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

void DrawPixel(QPainter &p, Layers &L, double phase) {
	if (L.pixels.empty()) {
		L.pixels.resize(kPixelStates);
	}
	const auto idx = std::clamp(
		int(std::floor(phase * kPixelStates)),
		0,
		kPixelStates - 1);
	if (L.pixels[idx].isNull()) {
		L.pixels[idx] = PixelState(L, idx);
	}
	const auto cell = L.side / 30.;
	// The photo is a circle: fill the strip between it and the stepped
	// inner outline.
	StrokeCircle(
		p,
		QPointF(L.c, L.c),
		(12.4 - 2. + 0.25) * cell,
		Color(kOutline),
		0.55 * cell);
	p.save();
	p.setRenderHint(QPainter::SmoothPixmapTransform, false);
	p.drawImage(QRectF(0., 0., L.side, L.side), L.pixels[idx]);
	p.restore();
}

// ------------------------------------------------------------- solitaire

[[nodiscard]] double GemAngle(const Layers &L) {
	switch (L.look.gemPos) {
	case 1: return -kPi / 2.;
	case 2: return 0.;
	}
	return kPi / 2.;
}

[[nodiscard]] bool HasGem(const Layers &L) {
	return (L.look.gem >= 0) && (L.look.gem < int(kGemPal.size()));
}

// The two metal lines (stroked outlines), with a gap for the gem.
[[nodiscard]] QPainterPath SolitaireLines(const Layers &L) {
	const auto u = L.u;
	const auto c = QPointF(L.c, L.c);
	const auto a = 40.5 * u;
	const auto r1 = a + 0.6 * u;
	const auto r2 = a + 4.6 * u;
	const auto gem = HasGem(L);
	const auto bot = GemAngle(L);
	const auto gap = gem ? 0.23 : 0.;
	const auto gap2 = gem ? 0.19 : 0.;
	const auto inner = gem
		? ArcPath(c, r1, bot + gap2, bot - gap2 + kTau)
		: CirclePath(c, r1);
	const auto outer = gem
		? ArcPath(c, r2, bot + gap, bot - gap + kTau)
		: CirclePath(c, r2);
	auto result = StrokedPath(inner, 1.7 * u, Qt::RoundCap);
	result.addPath(StrokedPath(outer, 0.9 * u, Qt::RoundCap));
	result.setFillRule(Qt::WindingFill);
	return result;
}

void BuildSolitaire(Layers &L) {
	const auto u = L.u;
	const auto c = L.c;
	const auto a = 40.5 * u;
	const auto r1 = a + 0.6 * u;
	const auto r2 = a + 4.6 * u;
	const auto &M = PickPal(kMetalPal, L.look.palette);
	const auto bot = GemAngle(L);
	L.metalLines = SolitaireLines(L);

	// Under: the line shadows (the live metal covers the lines).
	L.under = MakeLayer(L.side);
	{
		auto p = QPainter(&L.under);
		PrepareHq(p);
		DrawWithShadow(
			p,
			QRectF(0., 0., L.side, L.side),
			L.side,
			1.5 * u,
			QPointF(),
			Color(0x000000, std::min(0.8, 0.45 * std::max(0.6, L.g))),
			1.,
			[&](QPainter &q) {
				q.setPen(Qt::NoPen);
				q.setBrush(Color(M[0]));
				q.drawPath(L.metalLines);
			});
	}

	// The metal: a conic gradient, rotated while drawing.
	L.metal = ConicImage(
		QRect(0, 0, L.side, L.side),
		QPointF(c, c),
		0.,
		Pal(M.begin(), M.begin() + 9),
		1.);

	// Over: accent dots, gem bezel and table.
	L.over = MakeLayer(L.side);
	auto p = QPainter(&L.over);
	PrepareHq(p);
	for (const auto angle : { bot + 0.68 * kPi, bot - 0.68 * kPi }) {
		FillCircle(
			p,
			QPointF(c + r2 * std::cos(angle), c + r2 * std::sin(angle)),
			1.1 * u,
			Color(M[9]));
	}
	if (HasGem(L)) {
		const auto &G = kGemPal[L.look.gem];
		// The stone hangs 1.2u outward only at the bottom.
		const auto off = (r1 + r2) / 2. + (L.look.gemPos ? 0. : 1.2 * u);
		L.jx = c + off * std::cos(bot);
		L.jy = c + off * std::sin(bot);
		const auto R = 5.4 * u;
		const auto rt = R * 0.52;
		const auto o = QPointF(L.jx, L.jy);
		DrawWithShadow(
			p,
			Circle(o, R * 1.22),
			L.side,
			R * 0.6,
			QPointF(),
			Color(0x000000, 0.55),
			1.,
			[&](QPainter &q) { FillCircle(q, o, R * 1.22, Color(M[10])); });
		auto bezel = QLinearGradient(
			L.jx,
			L.jy - R * 1.2,
			L.jx,
			L.jy + R * 1.2);
		bezel.setColorAt(0., Color(M[11]));
		bezel.setColorAt(1., Color(M[12]));
		p.setPen(Qt::NoPen);
		p.setBrush(bezel);
		p.drawPath(StrokedPath(CirclePath(o, R * 1.13), R * 0.18, Qt::FlatCap));

		const auto point = [&](double r, double angle) {
			return QPointF(L.jx + r * std::cos(angle), L.jy + r * std::sin(angle));
		};
		auto table = QPainterPath();
		for (auto i = 0; i != 8; ++i) {
			const auto angle = i / 8. * kTau - kPi / 2.;
			if (!i) {
				table.moveTo(point(rt, angle));
			} else {
				table.lineTo(point(rt, angle));
			}
		}
		table.closeSubpath();
		auto tableFill = QLinearGradient(
			L.jx - rt,
			L.jy - rt,
			L.jx + rt,
			L.jy + rt);
		tableFill.setColorAt(0., Color(G[10]));
		tableFill.setColorAt(1., Color(G[11]));
		p.setBrush(tableFill);
		p.drawPath(table);
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(Color(0x5A6473, 0.5), R * 0.06));
		p.drawPath(table);

		// Facets (live colours).
		L.facets.clear();
		for (auto i = 0; i != 8; ++i) {
			const auto a0 = i / 8. * kTau - kPi / 2.;
			const auto a1 = (i + 1) / 8. * kTau - kPi / 2.;
			const auto am = (a0 + a1) / 2.;
			auto tri = QPainterPath(point(R, a0));
			tri.lineTo(point(R, am));
			tri.lineTo(point(rt, a0));
			tri.closeSubpath();
			auto quad = QPainterPath(point(R, am));
			quad.lineTo(point(R, a1));
			quad.lineTo(point(rt, a1));
			quad.lineTo(point(rt, a0));
			quad.closeSubpath();
			L.facets.push_back(std::move(tri));
			L.facets.push_back(std::move(quad));
		}
		L.sparkle = SparkleImage(G[12]);
	}
	p.end();
}

void DrawSolitaire(QPainter &p, Layers &L, double phase) {
	const auto u = L.u;
	const auto c = L.c;
	p.drawImage(0, 0, L.under);
	auto brush = QBrush(L.metal);
	auto transform = QTransform();
	transform.translate(c, c);
	transform.rotateRadians(kTau * phase);
	transform.translate(-c, -c);
	brush.setTransform(transform);
	p.save();
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	p.setPen(Qt::NoPen);
	p.setBrush(brush);
	p.drawPath(L.metalLines);
	p.restore();
	p.drawImage(0, 0, L.over);
	if (L.facets.size() != 16) {
		return;
	}
	const auto &G = kGemPal[L.look.gem];
	const auto rot = kTau * phase / 8.;
	p.save();
	p.setPen(Qt::NoPen);
	for (auto i = 0; i != 8; ++i) {
		const auto am = (i + 0.5) / 8. * kTau - kPi / 2.;
		const auto lum = 0.5 + 0.5 * std::cos(am * 3. + rot * 8. + 0.6);
		p.setBrush(Color(LerpRgb(G[0], G[1], lum)));
		p.drawPath(L.facets[i * 2]);
		p.setBrush(Color(G[2 + (i + 3) % 8], 0.4 + 0.6 * (1. - lum)));
		p.drawPath(L.facets[i * 2 + 1]);
	}
	p.restore();
	const auto f = std::max(
		0.,
		std::sin(kPi * std::clamp((phase - 0.35) / 0.3, 0., 1.)));
	const auto k = std::clamp(L.g, 0.4, 1.3);
	DrawSparkle(
		p,
		L.sparkle,
		L.jx + 1.6 * u,
		L.jy - 1.8 * u,
		(1. + 7. * f) * u * k,
		kPi * phase,
		f);
}

// ------------------------------------------------------------- cosmos

struct CosmosStars {
	std::array<double, 42> angle = { { 0. } };
	std::array<double, 42> distance = { { 0. } };
	std::array<double, 42> offset = { { 0. } };
	std::array<double, 42> radius = { { 0. } };
	std::array<bool, 42> big = { { false } };
};

[[nodiscard]] const CosmosStars &Stars() {
	static const auto result = [] {
		auto stars = CosmosStars();
		auto r = Rng(3);
		for (auto i = 0; i != 42; ++i) {
			stars.angle[i] = r.next() * kTau;
			stars.distance[i] = 0.5 + r.next() * 10.5;
			stars.big[i] = r.next() < 0.18;
			stars.offset[i] = r.next();
			if (!stars.big[i]) {
				stars.radius[i] = 0.35 + 0.45 * r.next();
			}
		}
		return stars;
	}();
	return result;
}

void BuildCosmos(Layers &L) {
	const auto u = L.u;
	const auto c = QPointF(L.c, L.c);
	const auto a = 38.5 * u;
	const auto g = L.g;
	const auto &P = PickPal(kCosmosPal, L.look.palette);
	const auto neb = Pal{ P[0], P[1], P[2], P[3], P[0] };
	auto colors = Pal{ neb[0] };
	for (auto k = 0; k != 3; ++k) {
		colors.insert(colors.end(), neb.begin() + 1, neb.end());
	}
	L.rot1 = MakeLayer(L.side);
	{
		auto p = QPainter(&L.rot1);
		PrepareHq(p);
		DrawBlurred(
			p,
			L.side,
			3.5 * u * kBlurK,
			QPainter::CompositionMode_SourceOver,
			std::min(1., 0.35 + 0.65 * g),
			[&](QPainter &q) {
				StrokeConic(
					q,
					CirclePath(c, a + 3. * u),
					12. * u,
					Qt::FlatCap,
					c,
					0.,
					colors);
			});
		ClearPhoto(p, c, a);
	}
	L.rot2 = MakeLayer(L.side);
	{
		auto p = QPainter(&L.rot2);
		PrepareHq(p);
		DrawBlurred(
			p,
			L.side,
			1.2 * u * kBlurK,
			QPainter::CompositionMode_SourceOver,
			1.,
			[&](QPainter &q) {
				StrokeConic(
					q,
					CirclePath(c, a + 2.5 * u),
					4. * u,
					Qt::FlatCap,
					c,
					1.,
					colors);
			});
		ClearPhoto(p, c, a);
	}
	L.over = MakeLayer(L.side);
	{
		auto p = QPainter(&L.over);
		PrepareHq(p);
		StrokeCircle(p, c, a + 0.2 * u, Color(P[4], 0.75), 0.7 * u);
	}
	L.sparkle = SparkleImage(P[5]);

	// Ringed planet: back half of the ring, the planet, the front half.
	const auto ps = std::max(4, int(std::ceil(18. * u)));
	L.planet = MakeLayer(ps);
	auto p = QPainter(&L.planet);
	PrepareHq(p);
	const auto pc = QPointF(ps / 2., ps / 2.);
	const auto ringHalf = [&](double start, double alpha) {
		auto transform = QTransform();
		transform.translate(pc.x(), pc.y());
		transform.rotateRadians(-0.4);
		auto path = QPainterPath();
		for (auto i = 0; i <= 48; ++i) {
			const auto t = start + kPi * i / 48.;
			const auto point = transform.map(
				QPointF(6.2 * u * std::cos(t), 1.9 * u * std::sin(t)));
			if (!i) {
				path.moveTo(point);
			} else {
				path.lineTo(point);
			}
		}
		p.save();
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(Color(P[9], alpha), 0.8 * u));
		p.drawPath(path);
		p.restore();
	};
	ringHalf(kPi, 0.9);
	auto planet = QRadialGradient(
		QPointF(pc.x() - 1.2 * u, pc.y() - 1.2 * u),
		4.6 * u);
	planet.setColorAt(0.06, Color(P[6]));
	planet.setColorAt(0.5, Color(P[7]));
	planet.setColorAt(1., Color(P[8]));
	DrawWithShadow(
		p,
		Circle(pc, 3.4 * u),
		ps,
		4. * u * std::max(0.3, g),
		QPointF(),
		Color(P[10]),
		1.,
		[&](QPainter &q) {
			q.setPen(Qt::NoPen);
			q.setBrush(planet);
			q.drawEllipse(Circle(pc, 3.4 * u));
		});
	ringHalf(0., 0.95);
	p.end();
}

void DrawCosmos(QPainter &p, Layers &L, double phase) {
	const auto u = L.u;
	const auto c = L.c;
	const auto a = 38.5 * u;
	DrawRotated(p, L.rot1, c, kTau * phase / 3.);
	DrawRotated(
		p,
		L.rot2,
		c,
		-kTau * phase / 3.,
		std::min(1., 0.6 * L.g),
		QPainter::CompositionMode_Plus);
	p.drawImage(0, 0, L.over);
	const auto &stars = Stars();
	for (auto i = 0; i != 42; ++i) {
		const auto tw = 0.5 + 0.5 * std::sin(
			kTau * ((1 + (i % 3)) * phase + stars.offset[i]));
		const auto d = a + stars.distance[i] * u;
		const auto x = c + d * std::cos(stars.angle[i]);
		const auto y = c + d * std::sin(stars.angle[i]);
		if (stars.big[i]) {
			DrawSparkle(
				p,
				L.sparkle,
				x,
				y,
				(1.2 + 2.6 * tw) * u,
				0.,
				0.5 + 0.5 * tw);
		} else {
			FillCircle(
				p,
				QPointF(x, y),
				stars.radius[i] * u,
				Color(0xFFFFFF, 0.35 + 0.65 * tw));
		}
	}
	if (L.look.toggle1 && !L.planet.isNull()) {
		const auto pa = kTau * phase - kPi / 2.;
		const auto pr = a + 5. * u;
		const auto px = c + pr * std::cos(pa);
		const auto py = c + pr * std::sin(pa);
		p.drawImage(
			QPointF(
				px - L.planet.width() / 2.,
				py - L.planet.height() / 2.),
			L.planet);
	}
	if (L.look.toggle2) {
		const auto q = (phase - 0.55) / 0.2;
		if (q > 0. && q < 1.) {
			const auto an = -2.4 + q * 0.9;
			const auto d = a + 9. * u;
			const auto s = std::sin(kPi * q);
			// The tail: an arc fading in towards the head.
			constexpr auto kSegments = 12;
			p.save();
			for (auto i = 0; i != kSegments; ++i) {
				const auto t0 = i / double(kSegments);
				const auto t1 = (i + 1) / double(kSegments);
				const auto a0 = an - 0.6 + 0.6 * t0;
				const auto a1 = an - 0.6 + 0.6 * t1;
				auto pen = QPen(Color(0xFFFFFF, 0.9 * t1 * s), 0.8 * u);
				pen.setCapStyle(Qt::FlatCap);
				p.setPen(pen);
				p.drawLine(
					QPointF(c + d * std::cos(a0), c + d * std::sin(a0)),
					QPointF(c + d * std::cos(a1), c + d * std::sin(a1)));
			}
			p.restore();
			DrawSparkle(
				p,
				L.sparkle,
				c + d * std::cos(an),
				c + d * std::sin(an),
				2.2 * u,
				0.,
				s);
		}
	}
}

// ------------------------------------------------------------- frost

struct Flakes {
	std::array<double, 9> x = { { 0. } };
	std::array<double, 9> off = { { 0. } };
	std::array<double, 9> size = { { 0. } };
};

[[nodiscard]] const Flakes &FrostFlakes() {
	static const auto result = [] {
		auto flakes = Flakes();
		auto r = Rng(9);
		for (auto i = 0; i != 9; ++i) {
			flakes.x[i] = r.next();
			flakes.off[i] = r.next();
			flakes.size[i] = 1.6 + r.next() * 1.8;
		}
		return flakes;
	}();
	return result;
}

constexpr auto kIcicles = std::array<std::pair<double, double>, 6>{ {
	{ 1.32, 4.5 },
	{ 1.42, 6.5 },
	{ 1.5, 3.5 },
	{ 1.57, 7.5 },
	{ 1.66, 5. },
	{ 1.73, 3.5 },
} };

[[nodiscard]] QImage FlakeImage(QColor color, QColor glow) {
	auto result = MakeLayer(kFlakeSide);
	auto p = QPainter(&result);
	PrepareHq(p);
	const auto c = kFlakeSide / 2.;
	const auto r = kFlakeR;
	DrawWithShadow(
		p,
		Circle({ c, c }, r),
		kFlakeSide,
		r * 0.7,
		QPointF(),
		glow,
		1.,
		[&](QPainter &q) {
			auto path = QPainterPath();
			for (auto i = 0; i != 6; ++i) {
				auto t = QTransform();
				t.translate(c, c);
				t.rotate(60. * (i + 1));
				const auto line = [&](QPointF from, QPointF to) {
					path.moveTo(t.map(from));
					path.lineTo(t.map(to));
				};
				line({ 0., 0. }, { 0., -r });
				line({ 0., -r * 0.55 }, { r * 0.28, -r * 0.8 });
				line({ 0., -r * 0.55 }, { -r * 0.28, -r * 0.8 });
			}
			auto pen = QPen(color, r * 0.22);
			pen.setCapStyle(Qt::RoundCap);
			q.setPen(pen);
			q.setBrush(Qt::NoBrush);
			q.drawPath(path);
		});
	p.end();
	return result;
}

[[nodiscard]] QPainterPath CapPath(
		const Layers &L,
		double rr,
		double scale,
		double dy) {
	const auto u = L.u;
	const auto c = L.c;
	const auto a0 = kPi * 1.2;
	const auto a1 = kPi * 1.8;
	auto rc = Rng(4);
	auto path = QPainterPath();
	path.setFillRule(Qt::WindingFill);
	for (auto i = 0; i <= 11; ++i) {
		const auto k = i / 11.;
		const auto an = a0 + (a1 - a0) * k;
		const auto br = (2.6 + 3.2 * std::sin(kPi * k) + rc.next() * 0.8) * u;
		const auto d = rr + 1.2 * u + 0.4 * br;
		path.addEllipse(Circle(
			QPointF(c + d * std::cos(an), c + d * std::sin(an) + dy),
			br * scale));
	}
	return path.simplified();
}

void BuildFrost(Layers &L) {
	const auto u = L.u;
	const auto c = QPointF(L.c, L.c);
	const auto a = 38.5 * u;
	const auto rr = a + 1.5 * u;
	const auto g = L.g;
	const auto light = L.light;
	const auto cap = L.look.toggle2;
	const auto &P = PickPal(kFrostPal, L.look.palette);

	// Under: cold tint on the photo edge.
	L.under = MakeLayer(L.side);
	{
		auto p = QPainter(&L.under);
		PrepareHq(p);
		p.setPen(Qt::NoPen);
		p.setBrush(RadialFrom(c, a * 0.7, a, {
			{ 0., Color(P[8], 0.) },
			{ 1., Color(P[8], std::min(0.75, 0.45 * g)) },
		}));
		p.drawEllipse(Circle(c, a));
	}

	// The rotating ice ring with its glow.
	L.rot1 = MakeLayer(L.side);
	{
		auto p = QPainter(&L.rot1);
		PrepareHq(p);
		const auto ring = Pal(P.begin(), P.begin() + 7);
		const auto draw = [&](QPainter &q) {
			StrokeConic(
				q,
				CirclePath(c, rr),
				3.6 * u,
				Qt::FlatCap,
				c,
				0.,
				ring);
		};
		if (g > 0.01) {
			DrawWithShadow(
				p,
				Circle(c, rr + 2. * u),
				L.side,
				5. * u,
				QPointF(),
				Color(P[7], std::min(1., 0.8 * g)),
				1.,
				draw);
		} else {
			draw(p);
		}
		if (light) {
			// Light theme: a darker rim so the white ring reads on white.
			StrokeCircle(p, c, rr - 1.8 * u, Color(P[17], 0.75), 0.45 * u);
		}
	}

	// Over: outline, crystals, snow cap, icicles.
	L.over = MakeLayer(L.side);
	auto p = QPainter(&L.over);
	PrepareHq(p);
	StrokeCircle(
		p,
		c,
		rr + 1.9 * u,
		light ? Color(P[17], 0.95) : Color(P[15], 0.55),
		(light ? 0.8 : 0.5) * u);
	auto crystals = QPainterPath();
	auto r = Rng(21);
	for (auto i = 0; i != 26; ++i) {
		const auto an = r.next() * kTau;
		const auto len = (2.5 + r.next() * 5.) * u;
		if (cap && an > 3.75 && an < 5.65) {
			continue;
		}
		const auto d0 = rr + 1.2 * u;
		const auto dx = std::cos(an);
		const auto dy = std::sin(an);
		const auto x0 = c.x() + d0 * dx;
		const auto y0 = c.y() + d0 * dy;
		crystals.moveTo(x0, y0);
		crystals.lineTo(x0 + dx * len, y0 + dy * len);
		for (const auto sign : { 1., -1. }) {
			const auto bx = x0 + dx * len * 0.5;
			const auto by = y0 + dy * len * 0.5;
			const auto ba = an + sign * 0.7;
			crystals.moveTo(bx, by);
			crystals.lineTo(
				bx + std::cos(ba) * len * 0.4,
				by + std::sin(ba) * len * 0.4);
		}
	}
	DrawWithShadow(
		p,
		QRectF(0., 0., L.side, L.side),
		L.side,
		1. * u,
		QPointF(),
		Color(P[10], 0.6),
		1.,
		[&](QPainter &q) {
			auto pen = QPen(
				light ? Color(P[17], 0.95) : Color(P[9], 0.95),
				(light ? 0.7 : 0.55) * u);
			pen.setCapStyle(Qt::RoundCap);
			q.setPen(pen);
			q.setBrush(Qt::NoBrush);
			q.drawPath(crystals);
		});
	if (cap) {
		const auto base = CapPath(L, rr, 1., 0.);
		DrawWithShadow(
			p,
			base.boundingRect(),
			L.side,
			3. * u,
			QPointF(0., 1.2 * u),
			Color(0x001431, 0.5),
			1.,
			[&](QPainter &q) {
				q.setPen(Qt::NoPen);
				q.setBrush(Color(P[11]));
				q.drawPath(base);
			});
		if (light) {
			p.save();
			p.setBrush(Qt::NoBrush);
			p.setPen(QPen(Color(P[17], 0.6), 0.6 * u));
			p.drawPath(base);
			p.restore();
		}
		auto top = QLinearGradient(
			0.,
			c.y() - rr - 10. * u,
			0.,
			c.y() - rr + 4. * u);
		top.setColorAt(0., Color(0xFFFFFF));
		top.setColorAt(1., Color(P[12]));
		p.save();
		p.setPen(Qt::NoPen);
		p.setBrush(top);
		p.drawPath(CapPath(L, rr, 0.92, -0.9 * u));
		p.restore();
		for (const auto &[k, len] : kIcicles) {
			const auto an = kPi * k;
			const auto x = c.x() + (rr - 1.5 * u) * std::cos(an);
			const auto y = c.y() + (rr - 1.5 * u) * std::sin(an);
			auto path = QPainterPath(QPointF(x - 1.2 * u, y - 0.5 * u));
			path.lineTo(x + 1.2 * u, y - 0.5 * u);
			path.lineTo(x, y + len * u);
			path.closeSubpath();
			auto fill = QLinearGradient(x, y, x, y + len * u);
			fill.setColorAt(0., Color(P[13], 0.95));
			fill.setColorAt(1., Color(P[14], 0.4));
			p.save();
			p.setPen(Qt::NoPen);
			p.setBrush(fill);
			p.drawPath(path);
			p.restore();
		}
	}
	p.end();
	L.flake = FlakeImage(
		light ? Color(LerpRgb(P[17], 0xFFFFFF, 0.2)) : Color(0xFFFFFF),
		Color(P[16], light ? 0.3 : 0.9));
}

void DrawFrost(QPainter &p, Layers &L, double phase) {
	const auto u = L.u;
	const auto c = L.c;
	const auto a = 38.5 * u;
	const auto rr = a + 1.5 * u;
	const auto s = double(L.side);
	p.drawImage(0, 0, L.under);
	DrawRotated(p, L.rot1, c, kTau * phase);
	p.drawImage(0, 0, L.over);
	if (L.look.toggle2) {
		// A drip of light running down one icicle.
		const auto dq = std::fmod(phase * 2., 1.);
		const auto an = kPi * 1.57;
		const auto ix = c + (rr - 1.5 * u) * std::cos(an);
		const auto iy = c + (rr - 1.5 * u) * std::sin(an);
		FillCircle(
			p,
			QPointF(ix, iy + dq * 7. * u),
			0.7 * u,
			Color(0xFFFFFF, std::sin(kPi * dq)));
	}
	if (L.look.toggle1 && !L.flake.isNull()) {
		const auto &flakes = FrostFlakes();
		for (auto i = 0; i != 9; ++i) {
			const auto size = flakes.size[i] * u;
			const auto q = std::fmod(phase + flakes.off[i], 1.);
			const auto x = flakes.x[i] * s
				+ std::sin(kTau * (q + flakes.off[i])) * 3. * u;
			const auto y = -5. * u + q * (s + 10. * u);
			const auto inside = std::hypot(x - c, y - c) < a - 2. * u;
			p.save();
			p.setRenderHint(QPainter::SmoothPixmapTransform);
			p.setOpacity(p.opacity() * (inside ? 140. : 242.) / 255.);
			p.translate(x, y);
			p.rotate(360. * q / 6.);
			p.scale(size / kFlakeR, size / kFlakeR);
			p.drawImage(
				QPointF(-kFlakeSide / 2., -kFlakeSide / 2.),
				L.flake);
			p.restore();
		}
	}
}

// ------------------------------------------------------------- v3

// Neko / angel / devil (frames-v3/frames.js, Android FrameRenderer): a smaller
// photo, so ears, a halo with wings and horns fit into the box.
constexpr auto kV3Avatar = 34.;

// Neko: ear, ring (ear edge), inner ear / paw pads, glow, whiskers,
// light-theme outline.
const std::vector<Pal> kNekoPal = {
	{ 0x2b2b33, 0xffb3d1, 0xff9ec4, 0xff96c8, 0xffe1ee, 0xd06a96 },
	{ 0x16161a, 0x4a4a56, 0xff9ec4, 0x9a9ab4, 0xd8d8e6, 0x2a2a32 },
	{ 0xf6f6f8, 0xffffff, 0xffc4d8, 0xffffff, 0xffffff, 0x9a9aa6 },
	{ 0xe07a26, 0xffb066, 0xffd0a8, 0xff9a3c, 0xfff0dc, 0xb05a14 },
	{ 0x7a5ac4, 0xc9b3ff, 0xf0c8ff, 0xb08cff, 0xf4ecff, 0x6a4ab4 },
};
// Angel: halo light, halo mid, halo glow, ring, wing edge.
const std::vector<Pal> kAngelPal = {
	{ 0xfff6c9, 0xffcf33, 0xffd84d, 0xfff4c2, 0xd9e4ff },
	{ 0xffffff, 0xb9c0cc, 0xe6ecf5, 0xf2f4f7, 0xd2d8e2 },
	{ 0xffe6ef, 0xff8fb8, 0xffb3cf, 0xffe0ea, 0xffd6e6 },
	{ 0xe6f6ff, 0x6cbcff, 0x9fd8ff, 0xdff2ff, 0xcfe8ff },
};
// Devil: ring dark, ring bright, ring accent, horn base, horn tip, tail, glow.
const std::vector<Pal> kDevilPal = {
	{ 0x7a0010, 0xff2a2a, 0xff7a1a, 0x8a0012, 0xff3b30, 0xd0101e, 0xff2a1a },
	{ 0x3a0a5a, 0xb02aff, 0xff4fd8, 0x4a0a6a, 0xc04aff, 0x8a1ad0, 0xb02aff },
	{ 0x111111, 0x5a5a62, 0xff2a2a, 0x0a0a0c, 0x55555e, 0x2a2a30, 0xff2a1a },
	{ 0x0a4a10, 0x3aff4a, 0xc8ff3a, 0x0a5a14, 0x4aff5a, 0x20b030, 0x3aff4a },
};

[[nodiscard]] QImage GlowRingImage(
		const Layers &L,
		double r,
		double width,
		uint32_t color,
		uint32_t glow,
		double glowBlur) {
	auto result = MakeLayer(L.side);
	auto p = QPainter(&result);
	PrepareHq(p);
	const auto c = QPointF(L.c, L.c);
	const auto draw = [&](QPainter &q) {
		q.setBrush(Qt::NoBrush);
		q.setPen(QPen(Color(color), width));
		q.drawEllipse(Circle(c, r));
	};
	if (L.g > 0.01) {
		DrawWithShadow(
			p,
			Circle(c, r + width),
			L.side,
			glowBlur * L.g,
			QPointF(),
			Color(glow, std::min(1., 0.7 * L.g)),
			1.,
			draw);
	} else {
		draw(p);
	}
	return result;
}

[[nodiscard]] QPainterPath EarPath(double u, bool inner) {
	auto path = QPainterPath();
	if (inner) {
		path.moveTo(-5. * u, 1. * u);
		path.quadTo(QPointF(-2. * u, -9. * u), QPointF(1. * u, -11. * u));
		path.quadTo(QPointF(5. * u, -5. * u), QPointF(5.5 * u, 1. * u));
	} else {
		path.moveTo(-10. * u, 3. * u);
		path.quadTo(QPointF(-4. * u, -14. * u), QPointF(1. * u, -17. * u));
		path.quadTo(QPointF(7. * u, -8. * u), QPointF(10. * u, 3. * u));
	}
	path.closeSubpath();
	return path;
}

void BuildNeko(Layers &L) {
	const auto u = L.u;
	const auto c = L.c;
	const auto a = kV3Avatar * u;
	const auto &P = PickPal(kNekoPal, L.look.palette);
	L.rot1 = GlowRingImage(L, a + 1.6 * u, 2.4 * u, P[1], P[3], 3. * u);
	L.over = MakeLayer(L.side);
	auto p = QPainter(&L.over);
	PrepareHq(p);
	if (L.light) {
		StrokeCircle(p, { c, c }, a + 2.9 * u, Color(P[5], 0.8), 0.6 * u);
	}
	if (L.look.toggle1) {
		auto pen = QPen(Color(L.light ? P[5] : P[4]), 0.8 * u);
		pen.setCapStyle(Qt::RoundCap);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		for (auto s = -1; s <= 1; s += 2) {
			for (auto k = -1; k <= 1; ++k) {
				p.drawLine(
					QPointF(c + s * (a + 3. * u), c + 4. * u + k * 3. * u),
					QPointF(
						c + s * (a + 10. * u),
						c + 3. * u + k * 4.5 * u));
			}
		}
	}
}

void DrawNeko(QPainter &p, Layers &L, double phase) {
	const auto u = L.u;
	const auto c = L.c;
	const auto a = kV3Avatar * u;
	const auto &P = PickPal(kNekoPal, L.look.palette);
	// The right ear twitches twice per loop.
	const auto tw = std::pow(std::max(0., std::sin(kTau * phase * 2.)), 8.)
		* 0.25;
	const auto outer = EarPath(u, false);
	const auto inner = EarPath(u, true);
	for (auto s = -1; s <= 1; s += 2) {
		const auto base = -kPi / 2. + s * 0.62;
		p.save();
		p.translate(c + a * std::cos(base), c + a * std::sin(base));
		p.rotate((base + kPi / 2. + (s > 0 ? tw : 0.)) * 180. / kPi);
		p.setPen(Qt::NoPen);
		p.setBrush(Color(P[0]));
		p.drawPath(outer);
		auto pen = QPen(
			Color((L.light && P[0] == 0xf6f6f8) ? P[5] : P[1]),
			1.4 * u);
		pen.setJoinStyle(Qt::MiterJoin);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawPath(outer);
		p.setPen(Qt::NoPen);
		p.setBrush(Color(P[2]));
		p.drawPath(inner);
		p.restore();
	}
	p.drawImage(0, 0, L.rot1);
	p.drawImage(0, 0, L.over);
	if (L.look.toggle2) {
		const auto py = c + a + 2.5 * u + std::sin(kTau * phase) * 0.6 * u;
		p.save();
		p.setPen(Qt::NoPen);
		p.setBrush(L.light ? Color(P[5], 0.35) : QColor(0, 0, 0, 128));
		p.drawEllipse(QRectF(
			c - 5.4 * u,
			py - 3.6 * u,
			10.8 * u,
			8.2 * u));
		p.setBrush(Color(0xFFFFFF));
		p.drawEllipse(QRectF(c - 5. * u, py - 4. * u, 10. * u, 8. * u));
		p.setBrush(Color(P[2]));
		p.drawEllipse(QRectF(
			c - 2.2 * u,
			py - 0.9 * u,
			4.4 * u,
			3.4 * u));
		p.drawEllipse(Circle({ c - 2.6 * u, py - 2.2 * u }, 0.9 * u));
		p.drawEllipse(Circle({ c, py - 2.8 * u }, 0.9 * u));
		p.drawEllipse(Circle({ c + 2.6 * u, py - 2.2 * u }, 0.9 * u));
		p.restore();
	}
}

[[nodiscard]] QImage WingImage(const Layers &L, int s, const Pal &P) {
	const auto u = L.u;
	auto result = MakeLayer(L.side);
	auto p = QPainter(&result);
	PrepareHq(p);
	const auto base = QPointF(
		L.c + s * (kV3Avatar - 3.) * u,
		L.c - 4. * u);
	const auto each = [&](QPainter &q, const auto &draw) {
		for (auto i = 0; i != 4; ++i) {
			const auto half = 9.5 - i * 1.4;
			q.save();
			q.translate(base);
			q.translate(s * (5. + i * 3.2) * u, (-4. + i * 3.4) * u);
			q.rotate(s * (0.5 + i * 0.25) * 180. / kPi);
			draw(q, QRectF(-half * u, -3.2 * u, 2. * half * u, 6.4 * u));
			q.restore();
		}
	};
	const auto fill = [&](QPainter &q) {
		q.setPen(Qt::NoPen);
		q.setBrush(Color(0xFFFFFF));
		each(q, [&](QPainter &w, QRectF rect) {
			w.drawEllipse(rect);
		});
	};
	if (L.g > 0.01) {
		DrawWithShadow(
			p,
			Circle(base, 30. * u),
			L.side,
			4. * u * L.g,
			QPointF(),
			Color(P[4], std::min(1., 0.8 * L.g)),
			1.,
			fill);
	} else {
		fill(p);
	}
	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(
		L.light ? Color(0x9aa6bd) : Color(P[4]),
		(L.light ? 0.7 : 0.5) * u));
	each(p, [&](QPainter &w, QRectF rect) {
		w.drawEllipse(rect);
	});
	return result;
}

void BuildAngel(Layers &L) {
	const auto u = L.u;
	const auto c = L.c;
	const auto a = kV3Avatar * u;
	const auto &P = PickPal(kAngelPal, L.look.palette);
	L.rot1 = GlowRingImage(
		L,
		a + 1.2 * u,
		1.2 * u,
		L.light ? P[1] : P[3],
		P[2],
		4. * u);
	if (L.look.toggle1) {
		L.under = WingImage(L, -1, P);
		L.over = WingImage(L, 1, P);
	}
	// The halo is centred at (c, c) of its layer, drawn shifted per frame.
	L.rot2 = MakeLayer(L.side);
	{
		auto p = QPainter(&L.rot2);
		PrepareHq(p);
		auto gradient = QLinearGradient(c - 16. * u, 0., c + 16. * u, 0.);
		gradient.setColorAt(0., Color(P[1]));
		gradient.setColorAt(0.5, Color(P[0]));
		gradient.setColorAt(1., Color(P[1]));
		const auto rect = QRectF(c - 15. * u, c - 4. * u, 30. * u, 8. * u);
		const auto draw = [&](QPainter &q) {
			q.setBrush(Qt::NoBrush);
			q.setPen(QPen(QBrush(gradient), 2.6 * u));
			q.drawEllipse(rect);
		};
		if (L.g > 0.01) {
			DrawWithShadow(
				p,
				rect.adjusted(-2. * u, -2. * u, 2. * u, 2. * u),
				L.side,
				8. * u * L.g,
				QPointF(),
				Color(P[2], std::min(1., L.g)),
				1.,
				draw);
		} else {
			draw(p);
		}
	}
	L.sparkle = SparkleImage(P[2]);
}

void DrawAngel(QPainter &p, Layers &L, double phase) {
	const auto u = L.u;
	const auto c = L.c;
	const auto a = kV3Avatar * u;
	if (L.look.toggle1 && !L.under.isNull() && !L.over.isNull()) {
		const auto fl = std::sin(kTau * phase) * 0.12;
		const auto px = (kV3Avatar - 3.) * u;
		const auto py = c - 4. * u;
		auto clip = QPainterPath();
		clip.addRect(QRectF(0., 0., L.side, L.side));
		clip.addEllipse(Circle({ c, c }, a));
		p.save();
		p.setClipPath(clip);
		p.setRenderHint(QPainter::SmoothPixmapTransform);
		for (auto s = -1; s <= 1; s += 2) {
			p.save();
			p.translate(c + s * px, py);
			p.rotate(s * fl * 180. / kPi);
			p.translate(-(c + s * px), -py);
			p.drawImage(0, 0, (s < 0) ? L.under : L.over);
			p.restore();
		}
		p.restore();
	}
	p.drawImage(0, 0, L.rot1);
	const auto hy = c - a - 3. * u + std::sin(kTau * phase) * 1.2 * u;
	p.drawImage(QPointF(0., hy - c), L.rot2);
	if (L.look.toggle2 && !L.sparkle.isNull()) {
		const auto sv = std::sin(kTau * phase);
		DrawSparkle(
			p,
			L.sparkle,
			c + 13. * u * std::cos(kTau * phase),
			hy + 3.5 * u * sv,
			2.4 * u,
			0.,
			std::max(0., sv));
	}
}

void BuildDevil(Layers &L) {
	const auto u = L.u;
	const auto c = L.c;
	const auto a = kV3Avatar * u;
	const auto r = a + 1.8 * u;
	const auto center = QPointF(c, c);
	const auto &P = PickPal(kDevilPal, L.look.palette);
	L.rot1 = MakeLayer(L.side);
	{
		auto p = QPainter(&L.rot1);
		PrepareHq(p);
		StrokeConic(
			p,
			CirclePath(center, r),
			2. * u,
			Qt::FlatCap,
			center,
			0.,
			Pal{ P[0], P[1], P[2], P[1], P[0] });
	}
	// The glow of the ring, pulsed by alpha.
	L.rot2 = MakeLayer(L.side);
	{
		auto p = QPainter(&L.rot2);
		PrepareHq(p);
		DrawBlurred(
			p,
			L.side,
			4. * u * kBlurK,
			QPainter::CompositionMode_SourceOver,
			1.,
			[&](QPainter &q) {
				q.setBrush(Qt::NoBrush);
				q.setPen(QPen(
					Color(P[6], std::min(1., 0.9 * L.g)),
					3.5 * u));
				q.drawEllipse(Circle(center, r));
			});
	}
	L.over = MakeLayer(L.side);
	auto p = QPainter(&L.over);
	PrepareHq(p);
	for (auto s = -1; s <= 1; s += 2) {
		auto path = QPainterPath();
		path.moveTo(-4.5 * u, 2. * u);
		path.quadTo(
			QPointF(s * -2. * u, -8. * u),
			QPointF(s * 7. * u, -15. * u));
		path.quadTo(QPointF(s * 2. * u, -6. * u), QPointF(4.5 * u, 2. * u));
		path.closeSubpath();
		auto transform = QTransform();
		transform.translate(c + s * 13. * u, c - a + 3. * u);
		const auto horn = transform.map(path);
		auto gradient = QLinearGradient(
			transform.map(QPointF(0., 2. * u)),
			transform.map(QPointF(s * 6. * u, -15. * u)));
		gradient.setColorAt(0., Color(P[3]));
		gradient.setColorAt(1., Color(P[4]));
		const auto draw = [&](QPainter &q) {
			q.setPen(Qt::NoPen);
			q.setBrush(QBrush(gradient));
			q.drawPath(horn);
		};
		if (L.g > 0.01) {
			DrawWithShadow(
				p,
				horn.boundingRect(),
				L.side,
				4. * u * L.g,
				QPointF(),
				Color(P[6], std::min(1., 0.8 * L.g)),
				1.,
				draw);
		} else {
			draw(p);
		}
		if (L.light || P[3] == 0x0a0a0c) {
			p.setBrush(Qt::NoBrush);
			p.setPen(QPen(Color(P[6], 0.7), 0.5 * u));
			p.drawPath(horn);
		}
	}
}

void DrawDevil(QPainter &p, Layers &L, double phase) {
	const auto u = L.u;
	const auto c = L.c;
	const auto a = kV3Avatar * u;
	const auto &P = PickPal(kDevilPal, L.look.palette);
	const auto pulse = L.look.toggle2
		? 0.55 + 0.45 * (0.5 + 0.5 * std::sin(kTau * phase * 2.))
		: 1.;
	p.save();
	p.setOpacity(p.opacity() * pulse);
	p.drawImage(0, 0, L.rot2);
	p.restore();
	DrawRotated(p, L.rot1, c, -kTau * phase);
	p.drawImage(0, 0, L.over);
	if (L.look.toggle1) {
		const auto sw = std::sin(kTau * phase) * 2.5 * u;
		const auto x0 = c + a * 0.7;
		const auto y0 = c + a * 0.72;
		auto tail = QPainterPath();
		tail.moveTo(x0, y0);
		tail.cubicTo(
			QPointF(x0 + 8. * u, y0 + 10. * u),
			QPointF(x0 + 14. * u + sw, y0 - 2. * u),
			QPointF(x0 + 10. * u + sw, y0 - 8. * u));
		p.save();
		auto pen = QPen(Color(P[5]), 1.6 * u);
		pen.setCapStyle(Qt::RoundCap);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawPath(tail);
		const auto tx = x0 + 10. * u + sw;
		const auto ty = y0 - 8. * u;
		auto tip = QPainterPath();
		tip.moveTo(tx, ty - 3.5 * u);
		tip.lineTo(tx + 3. * u, ty + 1.5 * u);
		tip.lineTo(tx - 3. * u, ty + 1.5 * u);
		tip.closeSubpath();
		p.setPen(Qt::NoPen);
		p.setBrush(Color(P[1]));
		p.drawPath(tip);
		p.restore();
	}
}

// ------------------------------------------------------------- entry

void Build(Layers &L) {
	if (L.built) {
		return;
	}
	L.built = true;
	const auto &style = L.look.style;
	if (style == u"inferno"_q) {
		BuildInferno(L);
	} else if (style == u"solitaire"_q) {
		BuildSolitaire(L);
	} else if (style == u"cosmos"_q) {
		BuildCosmos(L);
	} else if (style == u"frost"_q) {
		BuildFrost(L);
	} else if (style == u"neko"_q) {
		BuildNeko(L);
	} else if (style == u"angel"_q) {
		BuildAngel(L);
	} else if (style == u"devil"_q) {
		BuildDevil(L);
	}
}

// Draws the frame at loop phase 0..1 in layer pixel coordinates.
void DrawLive(QPainter &p, Layers &L, double phase) {
	Build(L);
	const auto &style = L.look.style;
	if (style == u"inferno"_q) {
		DrawInferno(p, L, phase, true);
	} else if (style == u"pixel"_q) {
		DrawPixel(p, L, phase);
	} else if (style == u"solitaire"_q) {
		DrawSolitaire(p, L, phase);
	} else if (style == u"cosmos"_q) {
		DrawCosmos(p, L, phase);
	} else if (style == u"neko"_q) {
		DrawNeko(p, L, phase);
	} else if (style == u"angel"_q) {
		DrawAngel(p, L, phase);
	} else if (style == u"devil"_q) {
		DrawDevil(p, L, phase);
	} else {
		DrawFrost(p, L, phase);
	}
}

[[nodiscard]] QImage RenderFrame(Layers &L, double phase, bool poster) {
	auto result = MakeLayer(L.side);
	auto p = QPainter(&result);
	PrepareHq(p);
	if (L.look.style == u"inferno"_q && (poster || !L.stripReady.load())) {
		Build(L);
		InfernoFlames(p, L.side, L.look.palette, L.g, phase);
		DrawInferno(p, L, phase, false);
	} else {
		DrawLive(p, L, phase);
	}
	p.end();
	return result;
}

const QImage &Poster(Layers &L) {
	if (L.poster.isNull()) {
		L.poster = RenderFrame(L, 0., true);
	}
	return L.poster;
}

// ------------------------------------------------------------- cache

class Cache final {
public:
	[[nodiscard]] std::shared_ptr<Layers> get(
			const Look &look,
			bool light,
			int side) {
		const auto key = look.key
			+ '|' + QString::number(side)
			+ (light ? u"|l"_q : u"|d"_q);
		const auto i = _map.constFind(key);
		if (i != _map.cend()) {
			return i.value();
		}
		auto result = std::make_shared<Layers>(look, light, side);
		const auto bytes = result->bytes();
		if (_bytes + bytes > kCacheBytes) {
			_map.clear();
			_bytes = 0;
		}
		_map.insert(key, result);
		_bytes += bytes;
		return result;
	}
	void clear() {
		_map.clear();
		_bytes = 0;
	}

private:
	QHash<QString, std::shared_ptr<Layers>> _map;
	qint64 _bytes = 0;

};

[[nodiscard]] Cache &Layered() {
	static auto result = Cache();
	return result;
}

// The inferno strip is rendered off the main thread.
void RequestStrip(const std::shared_ptr<Layers> &layers) {
	if (!layers->stripSide || layers->stripRequested) {
		return;
	}
	layers->stripRequested = true;
	QThreadPool::globalInstance()->start([layers] {
		BuildInfernoStrip(*layers);
	});
}

// ------------------------------------------------------------- slots

struct Slot {
	QPointer<QWidget> widget;
	QRect rect;
	qint64 seen = 0;
};

class Animator final {
public:
	[[nodiscard]] bool acquire(QWidget *widget, QRect rect) {
		const auto now = Now();
		auto free = -1;
		for (auto i = 0; i != kMaxAnimated; ++i) {
			auto &slot = _slots[i];
			if (slot.widget == widget && slot.rect == rect) {
				slot.seen = now;
				return true;
			} else if (free < 0
				&& (!slot.widget || now - slot.seen > kSlotIdle)) {
				free = i;
			}
		}
		if (free < 0) {
			retryLater(widget, rect, now);
			return false;
		}
		_slots[free] = Slot{ widget, rect, now };
		ensureTimer();
		return true;
	}

private:
	void ensureTimer() {
		if (!_timer) {
			_timer = std::make_unique<QTimer>();
			_timer->setInterval(kFrameInterval);
			QObject::connect(_timer.get(), &QTimer::timeout, [this] {
				tick();
			});
		}
		if (!_timer->isActive()) {
			_timer->start();
		}
	}
	void tick() {
		const auto now = Now();
		auto active = false;
		for (auto &slot : _slots) {
			if (!slot.widget) {
				continue;
			} else if (now - slot.seen > kSlotIdle) {
				slot = Slot();
				continue;
			}
			active = true;
			slot.widget->update(slot.rect.marginsAdded({ 2, 2, 2, 2 }));
		}
		if (!active) {
			_timer->stop();
		}
	}
	void retryLater(QWidget *widget, QRect rect, qint64 now) {
		if (now - _lastRetry < kSlotRetry / 4) {
			return;
		}
		_lastRetry = now;
		const auto weak = QPointer<QWidget>(widget);
		QTimer::singleShot(kSlotRetry, widget, [=] {
			if (const auto strong = weak.data()) {
				strong->update(rect);
			}
		});
	}

	std::array<Slot, kMaxAnimated> _slots;
	std::unique_ptr<QTimer> _timer;
	qint64 _lastRetry = 0;

};

[[nodiscard]] Animator &Animations() {
	// Never destroyed: it may be used until the very end of the app.
	static const auto instance = new Animator();
	return *instance;
}

// Rendered sizes are rounded up, so animated geometry (profile collapse)
// reuses a few images scaled down instead of rendering on every frame.
int QuantizeSide(int side) {
	const auto step = (side <= 64) ? 8 : (side <= 160) ? 16 : 48;
	return std::clamp(((side + step - 1) / step) * step, 16, kMaxSide);
}

} // namespace

bool IsKnownAvatarFrameStyle(const QString &style) {
	return FrameParams::IsKnownStyle(style);
}

double AvatarFramePhotoRadius(const QString &style) {
	if (style == u"inferno"_q) {
		return 36.;
	} else if (style == u"pixel"_q) {
		return 10.4 * 100. / 30.;
	} else if (style == u"solitaire"_q) {
		return 40.5;
	} else if (style == u"neko"_q
		|| style == u"angel"_q
		|| style == u"devil"_q) {
		return kV3Avatar;
	}
	return 38.5;
}

QImage RenderAvatarFrame(
		const AvatarFrame &frame,
		int side,
		bool light,
		double phase) {
	if (!IsKnownAvatarFrameStyle(frame.style)) {
		return QImage();
	}
	Layers layers(
		ComputeLook(frame),
		light,
		std::clamp(side, 16, kMaxSide));
	if (layers.stripSide) {
		BuildInfernoStrip(layers);
	}
	return RenderFrame(layers, phase, false);
}

QColor AvatarFramePaletteColor(const QString &style, int index) {
	const auto pick = [&](const std::vector<Pal> &list, int at) {
		return Color(PickPal(list, index)[at]);
	};
	if (style == u"inferno"_q) {
		return pick(kInfernoPal, 3);
	} else if (style == u"pixel"_q) {
		return pick(kPixelPal, (index == 3) ? 4 : 0);
	} else if (style == u"solitaire"_q) {
		return pick(kMetalPal, (index == 3) ? 3 : 2);
	} else if (style == u"cosmos"_q) {
		return pick(kCosmosPal, 1);
	} else if (style == u"frost"_q) {
		return pick(kFrostPal, 3);
	} else if (style == u"neko"_q) {
		return pick(kNekoPal, (index == 1) ? 0 : 1);
	} else if (style == u"angel"_q) {
		return pick(kAngelPal, 1);
	} else if (style == u"devil"_q) {
		return pick(kDevilPal, 1);
	}
	return QColor(128, 128, 128);
}

QColor AvatarFrameGemColor(int index) {
	if (index < 0 || index >= int(kGemPal.size())) {
		return QColor(0, 0, 0, 0);
	}
	return Color(index ? kGemPal[index][1] : 0xDFE6F0);
}

void PaintAvatarFrame(
		QPainter &p,
		const AvatarFrame &frame,
		QRectF photo,
		QRectF allowed,
		double opacity,
		bool animate,
		bool light) {
	if (!IsKnownAvatarFrameStyle(frame.style)
		|| photo.width() < 12.
		|| opacity <= 0.) {
		return;
	}
	const auto look = ComputeLook(frame);
	const auto natural = photo.width() * 50.
		/ AvatarFramePhotoRadius(frame.style);
	const auto center = photo.center();
	const auto room = 2. * std::min({
		center.x() - allowed.left(),
		allowed.right() - center.x(),
		center.y() - allowed.top(),
		allowed.bottom() - center.y() });
	const auto boxSide = std::clamp(room, natural * kMinFit, natural);
	const auto device = p.device();
	const auto ratio = device ? device->devicePixelRatioF() : 1.;
	const auto transform = p.transform();
	const auto scale = std::clamp(
		std::sqrt(std::abs(transform.determinant())),
		0.25,
		4.);
	const auto pixelSide = QuantizeSide(
		int(std::ceil(boxSide * ratio * scale)));
	const auto target = QRectF(
		center.x() - boxSide / 2.,
		center.y() - boxSide / 2.,
		boxSide,
		boxSide);

	const auto layers = Layered().get(look, light, pixelSide);
	auto live = false;
	if (animate
		&& look.anim
		&& device
		&& device->devType() == QInternal::Widget) {
		const auto widget = static_cast<QWidget*>(device);
		const auto rect = transform.mapRect(target).toAlignedRect();
		if (Animations().acquire(widget, rect)) {
			if (layers->stripSide
				&& !layers->stripReady.load(std::memory_order_acquire)) {
				RequestStrip(layers);
			} else {
				live = true;
			}
		}
	}
	p.save();
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	p.setOpacity(p.opacity() * opacity);
	if (live) {
		const auto image = RenderFrame(
			*layers,
			Phase(Now(), look.speed),
			false);
		p.drawImage(target, image);
	} else {
		p.drawImage(target, Poster(*layers));
	}
	p.restore();
}

void ClearAvatarFramesCache() {
	Layered().clear();
}

} // namespace StaticGram
