/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_profile_gift_bubbles.h"

#include "base/event_filter.h"
#include "chat_helpers/stickers_lottie.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_peer.h"
#include "data/data_star_gift.h"
#include "lang/lang_keys.h"
#include "lottie/lottie_animation.h"
#include "lottie/lottie_multi_player.h"
#include "lottie/lottie_single_player.h"
#include "main/main_session.h"
#include "settings/settings_credits_graphics.h"
#include "ui/effects/animation_value.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "window/window_session_controller.h"
#include "styles/style_basic.h"
#include "styles/style_credits.h"
#include "styles/style_info_profile_top_bar.h"

#include <QtGui/QLinearGradient>
#include <QtGui/QPainterPath>
#include <QtGui/QRadialGradient>
#include <QtGui/QWindow>
#include <QtWidgets/QApplication>

#include <array>
#include <cmath>

namespace StaticGram {
namespace {

constexpr auto kPi = 3.14159265358979323846;

// Geometry, in px at the full avatar size (desktop avatar is 80px, the
// Android one 96dp, so the Android numbers are scaled by ~0.83).
constexpr auto kBubbleRadius = 16.;
constexpr auto kHoverLift = 1.12; // Hover only lifts a bubble a little.
constexpr auto kHeldScale = 3.55; // Android HELD_GIFT_SCALE.
constexpr auto kGrowDistance = 92.; // Android 110dp from the avatar edge.
constexpr auto kMargin = 6.;
constexpr auto kMaxGlowSpread = 26.;
constexpr auto kDragThreshold = 5;
constexpr auto kReturnArc = 22.;

// The glass layers are drawn in a unit space (radius 1).
constexpr auto kGlowExtent = 2.25;
constexpr auto kBodyExtent = 1.12;
constexpr auto kBodyHalfWidth = 1.08;
constexpr auto kBodyHalfHeight = 1.07;

// Glass is rendered once for the orbit size and once for the scene size.
constexpr auto kSmallBucket = 20.;
constexpr auto kLargeBucket = 104.;

constexpr auto kArtFraction = 1.3; // Artwork side / bubble radius.
constexpr auto kLargeArtFraction = 1.15;

constexpr auto kIdleFrame = crl::time(33); // ~30 fps for the slow orbit.
constexpr auto kAppearDuration = crl::time(320);
constexpr auto kLiftDuration = crl::time(180);
constexpr auto kGrabDuration = crl::time(210);
constexpr auto kReturnDuration = crl::time(480);
constexpr auto kSceneOpenDuration = crl::time(420);
constexpr auto kSceneCloseDuration = crl::time(300);
constexpr auto kSceneButtonGap = 24;
constexpr auto kSceneButtonHeight = 40;
constexpr auto kSceneButtonPadding = 22;
constexpr auto kSceneDimAlpha = 150;

enum class GlassLayer {
	Glow,
	Body,
	Highlight,
};

[[nodiscard]] QColor MultAlpha(QColor color, float64 alpha) {
	color.setAlphaF(color.alphaF() * alpha);
	return color;
}

[[nodiscard]] QColor White(int alpha) {
	return QColor(255, 255, 255, alpha);
}

[[nodiscard]] float64 SafeClamp(float64 value, float64 min, float64 max) {
	return (min > max) ? ((min + max) / 2.) : std::clamp(value, min, max);
}

[[nodiscard]] float64 Lerp(float64 a, float64 b, float64 t) {
	return a + (b - a) * t;
}

// A slightly irregular circle, so the bubble reads as liquid glass.
[[nodiscard]] const QPainterPath &Contour() {
	static const auto result = [] {
		auto path = QPainterPath();
		path.moveTo(-0.96, -0.19);
		path.cubicTo(-0.91, -0.75, -0.55, -1.03, -0.03, -0.99);
		path.cubicTo(0.48, -1.04, 1.02, -0.65, 1.01, -0.08);
		path.cubicTo(1.07, 0.54, 0.65, 0.98, 0.12, 0.99);
		path.cubicTo(-0.53, 1.04, -1.06, 0.49, -0.96, -0.19);
		path.closeSubpath();
		return path;
	}();
	return result;
}

[[nodiscard]] const QPainterPath &Reflection() {
	static const auto result = [] {
		auto path = QPainterPath();
		path.moveTo(-0.83, -0.38);
		path.cubicTo(-0.69, -0.73, -0.4, -0.94, -0.03, -0.91);
		path.cubicTo(-0.36, -0.81, -0.64, -0.65, -0.83, -0.38);
		path.closeSubpath();
		return path;
	}();
	return result;
}

// Port of ProfileGiftGlass (Android) with the beat pulse at rest.
[[nodiscard]] QImage RenderGlass(
		GlassLayer layer,
		const QColor &color,
		const QColor &edge,
		float64 radius) {
	const auto ratio = style::DevicePixelRatio();
	const auto extent = (layer == GlassLayer::Glow)
		? kGlowExtent
		: kBodyExtent;
	const auto side = int(std::ceil(2. * extent * radius));
	auto result = QImage(
		QSize(side, side) * ratio,
		QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(ratio);
	result.fill(Qt::transparent);

	const auto light = anim::color(color, White(255), 0.18);
	auto p = QPainter(&result);
	auto hq = PainterHighQualityEnabler(p);
	p.translate(side / 2., side / 2.);
	p.scale(radius, radius);
	p.setPen(Qt::NoPen);
	switch (layer) {
	case GlassLayer::Glow: {
		auto outer = QRadialGradient(QPointF(), 2.25);
		outer.setColorAt(0., MultAlpha(color, 0.38));
		outer.setColorAt(0.47, MultAlpha(color, 0.29));
		outer.setColorAt(0.72, MultAlpha(edge, 0.12));
		outer.setColorAt(1., MultAlpha(edge, 0.));
		p.setOpacity(0.38);
		p.setBrush(outer);
		p.drawEllipse(QPointF(), 2.25, 2.25);

		auto glow = QRadialGradient(QPointF(), 1.62);
		glow.setColorAt(0., MultAlpha(light, 0.8));
		glow.setColorAt(0.54, MultAlpha(light, 0.74));
		glow.setColorAt(0.68, MultAlpha(color, 0.46));
		glow.setColorAt(0.84, MultAlpha(edge, 0.15));
		glow.setColorAt(1., MultAlpha(edge, 0.));
		p.setOpacity(0.79);
		p.setBrush(glow);
		p.drawEllipse(QPointF(), 1.62, 1.62);
	} break;
	case GlassLayer::Body: {
		auto body = QRadialGradient(QPointF(-0.3, -0.39), 1.65);
		body.setColorAt(0., White(0x72));
		body.setColorAt(0.31, MultAlpha(light, 0.29));
		body.setColorAt(0.68, MultAlpha(color, 0.36));
		body.setColorAt(1., MultAlpha(edge, 0.82));
		p.setBrush(body);
		p.drawPath(Contour());
	} break;
	case GlassLayer::Highlight: {
		const auto area = QRectF(-1.2, -1.2, 2.4, 2.4);
		p.setClipPath(Contour());

		auto sheen = QRadialGradient(QPointF(-0.42, -0.65), 0.7);
		sheen.setColorAt(0., White(0xB8));
		sheen.setColorAt(0.25, White(0x52));
		sheen.setColorAt(1., White(0));
		p.fillRect(area, sheen);

		auto caustic = QRadialGradient(QPointF(0.36, 0.87), 0.84);
		caustic.setColorAt(0., White(0xA5));
		caustic.setColorAt(0.39, MultAlpha(light, 0.53));
		caustic.setColorAt(1., MultAlpha(light, 0.));
		p.setOpacity(0.62);
		p.fillRect(area, caustic);

		auto reflection = QLinearGradient(-0.78, -0.4, -0.12, -0.96);
		reflection.setColorAt(0., White(0));
		reflection.setColorAt(0.5, White(0xD4));
		reflection.setColorAt(1., White(0x22));
		p.setOpacity(220. / 255.);
		p.setBrush(reflection);
		p.drawPath(Reflection());

		p.setClipping(false);
		p.setOpacity(1.);
		auto rim = QLinearGradient(-0.72, -0.78, 0.78, 0.86);
		rim.setColorAt(0., White(0xF2));
		rim.setColorAt(0.29, MultAlpha(light, 0.65));
		rim.setColorAt(0.63, White(0x2A));
		rim.setColorAt(1., White(0xC4));
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(QBrush(rim), std::max(0.025, 1. / radius)));
		p.drawPath(Contour());
	} break;
	}
	p.end();
	return result;
}

} // namespace

struct ProfileGiftBubbles::GlassImages {
	QImage glow;
	QImage body;
	QImage highlight;
	int ratio = 0;

	[[nodiscard]] bool valid() const {
		return !body.isNull() && (ratio == style::DevicePixelRatio());
	}
	void render(const QColor &color, const QColor &edge, float64 radius) {
		glow = RenderGlass(GlassLayer::Glow, color, edge, radius);
		body = RenderGlass(GlassLayer::Body, color, edge, radius);
		highlight = RenderGlass(GlassLayer::Highlight, color, edge, radius);
		ratio = style::DevicePixelRatio();
	}
};

struct ProfileGiftBubbles::Bubble {
	std::optional<Data::SavedStarGift> data;
	uint64 id = 0;
	QColor color;
	QColor edge;
	int slot = 0;
	crl::time shownAt = 0;
	std::shared_ptr<Data::DocumentMedia> media;
	Lottie::Animation *animation = nullptr;
	GlassImages glass;

	// Where it was drawn last time, for the hit test.
	QPointF drawnCenter;
	float64 drawnRadius = 0.;
	bool drawnFront = false;
};

struct ProfileGiftBubbles::Large {
	uint64 giftId = 0;
	GlassImages glass;
	std::shared_ptr<Data::DocumentMedia> media;
	std::unique_ptr<Lottie::SinglePlayer> player;
	rpl::lifetime lifetime;
};

// Above the header children while a bubble is held or flies back: paints
// it over the name and the buttons, never takes the mouse.
class ProfileGiftBubbles::HeldLayer final : public Ui::RpWidget {
public:
	HeldLayer(QWidget *parent, Fn<void(QPainter&)> paint)
	: RpWidget(parent)
	, _paint(std::move(paint)) {
		setAttribute(Qt::WA_TransparentForMouseEvents);
	}

protected:
	void paintEvent(QPaintEvent *e) override {
		auto p = QPainter(this);
		_paint(p);
	}

private:
	const Fn<void(QPainter&)> _paint;

};

// The full-window scene: even dim, the bubble flies from its place on the
// orbit to the window center and inflates, "Open" right under it.
class ProfileGiftBubbles::Scene final : public Ui::RpWidget {
public:
	struct Callbacks {
		Fn<void(QPainter&, QPointF, float64, float64, float64)> paintBubble;
		Fn<QRectF()> origin; // In this widget coordinates.
		Fn<void()> open;
		Fn<void()> finished;
	};

	Scene(QWidget *parent, Callbacks &&callbacks)
	: RpWidget(parent)
	, _callbacks(std::move(callbacks))
	, _button(tr::lng_open_link(tr::now))
	, _buttonWidth(st::semiboldFont->width(_button) + 2 * kSceneButtonPadding)
	, _previousFocus(QApplication::focusWidget()) {
		setGeometry(parent->rect());
		setMouseTracking(true);
		setFocusPolicy(Qt::StrongFocus);
		base::install_event_filter(this, parent, [=](not_null<QEvent*> e) {
			if (e->type() == QEvent::Resize) {
				setGeometry(parentWidget()->rect());
			}
			return base::EventFilterResult::Continue;
		});
		_wobble.init([=](crl::time now) {
			if (!isVisible() || window()->isMinimized()) {
				return false;
			}
			updateBubble();
			return true;
		});
		show();
		raise();
		setFocus();
		_started = crl::now();
		transition(1.);
	}

	// The scene is transparent over the whole window: outside of the
	// open / close transition only the bubble area is repainted, so the
	// widgets under it are not redrawn every frame.
	void updateBubble() {
		if (_progress.animating() || _layoutFor != size()) {
			update();
			return;
		}
		const auto extent = _radius + kMaxGlowSpread + 2.;
		const auto center = _center + _offset;
		update(QRect(
			int(std::floor(center.x() - extent)),
			int(std::floor(center.y() - extent)),
			int(std::ceil(2. * extent)),
			int(std::ceil(2. * extent))));
	}

	void close() {
		if (_closing) {
			return;
		}
		_closing = true;
		transition(0.);
	}

	void openAndClose() {
		if (_closing) {
			return;
		}
		_closing = true;
		hide();
		finish(true);
	}

protected:
	void paintEvent(QPaintEvent *e) override {
		auto p = QPainter(this);
		const auto progress = _progress.value(_closing ? 0. : 1.);
		if (progress <= 0.) {
			return;
		}
		if (!_wobble.animating() && !anim::Disabled()) {
			_wobble.start();
		}
		p.fillRect(rect(), QColor(9, 12, 24, int(kSceneDimAlpha * progress)));

		layout();
		const auto from = _callbacks.origin();
		const auto fromRadius = from.width() / 2.;
		const auto radius = Lerp(fromRadius, _radius, progress);
		const auto target = _center + _offset;
		const auto center = QPointF(
			Lerp(from.center().x(), target.x(), progress),
			Lerp(from.center().y(), target.y(), progress));
		const auto t = anim::Disabled()
			? 0.
			: (crl::now() - _started) / 1000.;
		const auto tilt = std::sin(1.45 * t) * 6.;
		_callbacks.paintBubble(p, center, radius, progress, tilt);

		p.setOpacity(progress);
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(_buttonOver ? White(0x4A) : White(0x2E));
		const auto r = _buttonRect.height() / 2.;
		p.drawRoundedRect(_buttonRect, r, r);
		p.setPen(White(255));
		p.setFont(st::semiboldFont);
		p.drawText(_buttonRect, _button, style::al_center);
		p.setOpacity(1.);
	}

	void keyPressEvent(QKeyEvent *e) override {
		if (e->key() == Qt::Key_Escape) {
			close();
		} else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
			openAndClose();
		}
		e->accept();
	}

	void mouseMoveEvent(QMouseEvent *e) override {
		if (_pressedOn == Hit::Bubble && !_closing) {
			// The bubble can be dragged (letting go closes the scene),
			// "Open" stays right under it.
			const auto d = e->pos() - _down;
			_dragged = _dragged || (d.manhattanLength() > kDragThreshold);
			_offset = QPointF(d) * 0.88;
			_buttonRect = _buttonBase.translated(_offset.toPoint());
			update();
			return;
		}
		const auto over = _buttonRect.contains(e->pos());
		if (_buttonOver != over) {
			_buttonOver = over;
			setCursor(over ? style::cur_pointer : style::cur_default);
			update(_buttonRect);
		}
	}

	void mousePressEvent(QMouseEvent *e) override {
		_pressedOn = hit(e->pos());
		_down = e->pos();
		_dragged = false;
	}

	void mouseReleaseEvent(QMouseEvent *e) override {
		const auto on = hit(e->pos());
		const auto pressed = std::exchange(_pressedOn, Hit::None);
		if (pressed == Hit::Bubble) {
			if (std::exchange(_dragged, false)) {
				close();
			}
			return;
		} else if (on != pressed) {
			return;
		} else if (on == Hit::Button) {
			openAndClose();
		} else if (on == Hit::Outside) {
			close();
		}
	}

private:
	enum class Hit {
		None,
		Outside,
		Bubble,
		Button,
	};

	[[nodiscard]] Hit hit(QPoint point) const {
		if (_buttonRect.contains(point)) {
			return Hit::Button;
		}
		const auto d = QPointF(point) - (_center + _offset);
		return (d.x() * d.x() + d.y() * d.y() <= _radius * _radius)
			? Hit::Bubble
			: Hit::Outside;
	}

	void layout() {
		const auto w = width();
		const auto h = height();
		if (_layoutFor == size()) {
			return;
		}
		_layoutFor = size();
		_radius = std::max(
			kBubbleRadius,
			std::min({ kLargeBucket, w * 0.34, h * 0.22 }));
		_center = QPointF(w / 2., h / 2.);
		const auto bubbleBottom = _center.y() + _radius * kBodyHalfHeight;
		const auto top = std::min(
			int(std::round(bubbleBottom)) + kSceneButtonGap,
			h - 16 - kSceneButtonHeight);
		_buttonBase = QRect(
			(w - _buttonWidth) / 2,
			top,
			_buttonWidth,
			kSceneButtonHeight);
		_buttonRect = _buttonBase.translated(_offset.toPoint());
	}

	void transition(float64 to) {
		_progress.start(
			[=](float64 value) {
				update();
				if (to == 0. && value <= 0.) {
					finish(false);
				}
			},
			_progress.value((to > 0.) ? 0. : 1.),
			to,
			(to > 0.) ? kSceneOpenDuration : kSceneCloseDuration,
			anim::easeOutQuint);
		if (anim::Disabled() && to == 0.) {
			finish(false);
		}
	}

	void finish(bool open) {
		if (_finishing) {
			return;
		}
		_finishing = true;
		_wobble.stop();
		if (!open && _previousFocus) {
			_previousFocus->setFocus();
		}
		// The owner destroys us, not from inside our own event handler.
		crl::on_main(this, [=, callbacks = _callbacks] {
			if (open) {
				callbacks.open();
			}
			callbacks.finished();
		});
	}

	const Callbacks _callbacks;
	const QString _button;
	const int _buttonWidth = 0;
	const QPointer<QWidget> _previousFocus;
	Ui::Animations::Simple _progress;
	Ui::Animations::Basic _wobble;
	crl::time _started = 0;
	QSize _layoutFor;
	QPointF _center;
	float64 _radius = 0.;
	QRect _buttonBase;
	QRect _buttonRect;
	QPointF _offset;
	QPoint _down;
	bool _dragged = false;
	bool _buttonOver = false;
	Hit _pressedOn = Hit::None;
	bool _closing = false;
	bool _finishing = false;

};

ProfileGiftBubbles::ProfileGiftBubbles(Descriptor &&descriptor)
: _header(descriptor.header)
, _controller(descriptor.controller)
, _peer(descriptor.peer)
, _userpicGeometry(std::move(descriptor.userpicGeometry))
, _progress(std::move(descriptor.progress))
, _contentTop(std::move(descriptor.contentTop))
, _contentBottom(std::move(descriptor.contentBottom)) {
	_animation.init([=](crl::time now) { return tick(now); });

	_header->setMouseTracking(true);
	base::install_event_filter(_header, [=](not_null<QEvent*> e) {
		return handleMouse(_header.get(), e.get(), false)
			? base::EventFilterResult::Cancel
			: base::EventFilterResult::Continue;
	}, _lifetime);
	if (const auto userpic = descriptor.userpicButton) {
		// The near half of the orbit passes over the avatar button.
		base::install_event_filter(userpic, [=](not_null<QEvent*> e) {
			return handleMouse(userpic, e.get(), true)
				? base::EventFilterResult::Cancel
				: base::EventFilterResult::Continue;
		}, _lifetime);
	}
}

ProfileGiftBubbles::~ProfileGiftBubbles() {
	_scene = nullptr;
	_heldLayer = nullptr;
	_large = nullptr;
	for (const auto &bubble : _bubbles) {
		if (bubble->animation && _player) {
			_player->remove(bubble->animation);
		}
	}
	_bubbles.clear();
	_player = nullptr;
}

bool ProfileGiftBubbles::empty() const {
	return _bubbles.empty();
}

void ProfileGiftBubbles::setGifts(
		const std::vector<Data::SavedStarGift> &gifts) {
	// Android ORBIT_POSITIONS.
	constexpr auto kSlots = std::array<int, 6>{ 1, 4, 2, 5, 0, 3 };

	auto now = std::vector<std::unique_ptr<Bubble>>();
	now.reserve(std::min(gifts.size(), kSlots.size()));
	const auto at = crl::now();
	for (const auto &gift : gifts) {
		if (now.size() >= kSlots.size()) {
			break;
		}
		const auto &unique = gift.info.unique;
		if (!unique) {
			continue;
		}
		const auto id = unique->id;
		const auto i = ranges::find(_bubbles, id, [](const auto &bubble) {
			return bubble ? bubble->id : uint64(0);
		});
		auto bubble = (i != end(_bubbles))
			? std::move(*i)
			: std::make_unique<Bubble>();
		const auto fallback = st::windowActiveTextFg->c;
		const auto color = unique->backdrop.centerColor.isValid()
			? unique->backdrop.centerColor
			: fallback;
		const auto edge = unique->backdrop.edgeColor.isValid()
			? unique->backdrop.edgeColor
			: fallback;
		if (bubble->color != color || bubble->edge != edge) {
			bubble->glass = GlassImages();
		}
		bubble->data = gift;
		bubble->id = id;
		bubble->color = color;
		bubble->edge = edge;
		bubble->slot = kSlots[now.size()];
		if (!bubble->shownAt) {
			bubble->shownAt = anim::Disabled() ? (at - kAppearDuration) : at;
		}
		if (!bubble->media) {
			bubble->media = gift.info.document->createMediaView();
			bubble->media->checkStickerSmall();
		}
		now.push_back(std::move(bubble));
	}
	for (const auto &old : _bubbles) {
		if (old && old->animation && _player) {
			_player->remove(old->animation);
		}
	}
	_bubbles = std::move(now);

	if (_hoveredId && !findBubble(_hoveredId)) {
		setHovered(-1);
	}
	if (_liftId && !findBubble(_liftId)) {
		_liftId = 0;
	}
	if (_pressedId && !findBubble(_pressedId)) {
		_pressedId = 0;
	}
	if (_heldId && !findBubble(_heldId)) {
		clearHold();
	}
	if (_sceneId && !findBubble(_sceneId)) {
		_scene = nullptr;
		_sceneId = 0;
	}
	if (_large && !findBubble(_large->giftId)) {
		_large = nullptr;
	}

	if (_bubbles.empty()) {
		_loadingLifetime.destroy();
		_player = nullptr;
		_animation.stop();
		_header->update();
		return;
	}
	if (!_player) {
		_player = std::make_unique<Lottie::MultiPlayer>(
			Lottie::Quality::Default);
		_player->updates() | rpl::on_next([=] {
			// Frames only matter while the header is really shown.
			if (canAnimate()) {
				updateArea();
			}
		}, _lifetime);
	}
	_loadingLifetime.destroy();
	rpl::single(rpl::empty_value()) | rpl::then(
		_peer->session().downloaderTaskFinished()
	) | rpl::on_next([=] {
		checkLoaded();
	}, _loadingLifetime);
	updateArea();
	ensureAnimating();
}

void ProfileGiftBubbles::checkLoaded() {
	auto allLoaded = true;
	for (const auto &bubble : _bubbles) {
		if (bubble->animation) {
			continue;
		} else if (bubble->media->loaded()) {
			setupLottie(*bubble);
		} else {
			allLoaded = false;
		}
	}
	if (allLoaded) {
		_loadingLifetime.destroy();
	}
	updateArea();
}

void ProfileGiftBubbles::setupLottie(Bubble &bubble) {
	const auto art = int(std::ceil(kSmallBucket * kArtFraction));
	bubble.animation = ChatHelpers::LottieAnimationFromDocument(
		_player.get(),
		bubble.media.get(),
		ChatHelpers::StickerLottieSize::PinnedProfileUniqueGiftSize,
		QSize(art, art) * style::DevicePixelRatio());
}

void ProfileGiftBubbles::ensureGlass(Bubble &bubble) {
	if (!bubble.glass.valid()) {
		bubble.glass.render(bubble.color, bubble.edge, kSmallBucket);
	}
}

void ProfileGiftBubbles::ensureLarge(Bubble &bubble) {
	if (_large && _large->giftId == bubble.id) {
		if (!_large->glass.valid()) {
			_large->glass.render(bubble.color, bubble.edge, kLargeBucket);
		}
		return;
	}
	_large = std::make_unique<Large>();
	_large->giftId = bubble.id;
	_large->glass.render(bubble.color, bubble.edge, kLargeBucket);
	_large->media = bubble.data->info.document->createMediaView();
	_large->media->checkStickerLarge();
	const auto large = _large.get();
	rpl::single(rpl::empty_value()) | rpl::then(
		_peer->session().downloaderTaskFinished()
	) | rpl::filter([=] {
		return large->media->loaded();
	}) | rpl::take(1) | rpl::on_next([=] {
		// The same size tag and box as the unique gift box uses,
		// so the frames cache is shared with it.
		const auto size = st::creditsHistoryEntryStarGiftSize;
		large->player = ChatHelpers::LottiePlayerFromDocument(
			large->media.get(),
			ChatHelpers::StickerLottieSize::MessageHistory,
			QSize(size, size),
			Lottie::Quality::High);
		large->player->updates() | rpl::on_next([=] {
			if (_scene) {
				_scene->updateBubble();
			}
			updateHeldLayer();
		}, large->lifetime);
	}, large->lifetime);
}

auto ProfileGiftBubbles::findBubble(uint64 giftId) const -> Bubble* {
	const auto i = ranges::find(_bubbles, giftId, [](const auto &bubble) {
		return bubble->id;
	});
	return (i != end(_bubbles)) ? i->get() : nullptr;
}

auto ProfileGiftBubbles::orbitPlacement(
		const Bubble &bubble,
		const QRect &userpic,
		float64 progress,
		crl::time now) const -> Placement {
	auto result = Placement();
	const auto full = float64(st::infoProfileTopBarPhotoSize);
	const auto k = userpic.width() / full;
	const auto t = anim::Disabled() ? 0. : (_orbitTime / 1000.);
	const auto id = bubble.id;
	const auto variant = float64(id & 3);
	const auto phase = float64(id & 127) * 0.31;
	const auto wave = (variant * 0.075 + 0.37) * t + phase;
	const auto angle = std::sin(wave) * 0.28
		+ 0.34 * t
		+ bubble.slot * kPi / 3.;
	const auto bob = (variant * 0.12 + 1.1) * t + phase;
	const auto depth = std::sin(angle);
	const auto acx = userpic.x() + userpic.width() / 2.;
	const auto acy = userpic.y() + userpic.height() / 2.;
	const auto rx = (std::sin(wave + angle) * 0.08 + 0.85)
		* std::min(std::max(0., acx - 28. * k), _header->width() * 0.32);
	const auto ry = (variant * 4. + 26.) * k;
	result.center = QPointF(
		std::sin(2. * angle + phase) * 9. * k + std::cos(angle) * rx + acx,
		std::sin(bob) * 6. * k + ry * depth + acy - 3. * k);
	result.depth = depth;
	result.tilt = anim::Disabled() ? 0. : (std::sin(bob) * 9.);

	const auto appear = std::clamp(
		(now - bubble.shownAt) / float64(kAppearDuration),
		0.,
		1.);
	const auto entered = anim::easeOutQuint(1., appear);
	result.radius = kBubbleRadius * k * (depth * 0.12 + 0.9)
		* (0.5 + 0.5 * entered);
	result.alpha = entered * std::clamp((progress - 0.5) / 0.5, 0., 1.);
	return result;
}

auto ProfileGiftBubbles::heldPlacement(
		const Bubble &bubble,
		const QRect &userpic,
		float64 progress,
		crl::time now) const -> Placement {
	const auto orbit = orbitPlacement(bubble, userpic, progress, now);
	auto result = orbit;
	auto weight = 0.;
	if (!_returnStarted) {
		// Pulled out of the orbit: follows the mouse and inflates only as
		// it gets away from the avatar.
		const auto g = anim::easeOutCubic(1., std::clamp(
			(now - _grabStarted) / float64(kGrabDuration),
			0.,
			1.));
		result.center = QPointF(
			Lerp(orbit.center.x(), _dragPosition.x(), g),
			Lerp(orbit.center.y(), _dragPosition.y(), g));
		const auto avatar = QRectF(userpic).center();
		const auto d = result.center - avatar;
		const auto away = std::sqrt(d.x() * d.x() + d.y() * d.y())
			- std::min(userpic.width(), userpic.height()) / 2.;
		const auto grow = g * std::clamp(away / kGrowDistance, 0., 1.);
		result.radius = Lerp(orbit.radius, kBubbleRadius * kHeldScale, grow);
		result.tilt = Lerp(orbit.tilt, 0., g);
		weight = g;
	} else {
		// Flies back to its place on the orbit on a small arc.
		const auto back = std::clamp(
			(now - _returnStarted) / float64(kReturnDuration),
			0.,
			1.);
		const auto e = anim::easeOutQuint(1., back);
		result.center = QPointF(
			Lerp(_released.center.x(), orbit.center.x(), e),
			Lerp(_released.center.y(), orbit.center.y(), e)
				- std::sin(e * kPi) * kReturnArc);
		result.radius = Lerp(_released.radius, orbit.radius, e);
		result.tilt = Lerp(0., orbit.tilt, e);
		weight = 1. - e;
	}

	// Inside the header: below the top bar, above the action buttons,
	// inside the sides. With too little room it inflates less, but never
	// shrinks below its orbit size.
	const auto limit = float64(_contentBottom());
	const auto top = std::min(float64(_contentTop()), limit);
	const auto w = float64(_header->width());
	const auto fit = std::min(
		std::max(0., (limit - top - 2. * kMargin) / 2.) / kBodyHalfHeight,
		(w / 2. - kMargin) / kBodyHalfWidth);
	if (result.radius > fit) {
		result.radius = std::max(fit, std::min(result.radius, orbit.radius));
	}
	const auto hw = result.radius * kBodyHalfWidth;
	const auto hh = result.radius * kBodyHalfHeight;
	const auto clamped = QPointF(
		SafeClamp(result.center.x(), kMargin + hw, w - kMargin - hw),
		SafeClamp(result.center.y(), top + kMargin + hh, limit - kMargin - hh));
	result.center = QPointF(
		Lerp(result.center.x(), clamped.x(), weight),
		Lerp(result.center.y(), clamped.y(), weight));
	result.depth = 1.;
	return result;
}

void ProfileGiftBubbles::paintBehind(QPainter &p, const QRect &clip) {
	ensureAnimating();
	paintOrbit(p, clip, false);
}

void ProfileGiftBubbles::paintFront(QPainter &p, const QRect &clip) {
	paintOrbit(p, clip, true);
}

void ProfileGiftBubbles::paintOrbit(
		QPainter &p,
		const QRect &clip,
		bool front) {
	if (_bubbles.empty()) {
		return;
	}
	const auto progress = _progress();
	if (progress <= 0.5) {
		for (const auto &bubble : _bubbles) {
			bubble->drawnRadius = 0.;
		}
		return;
	}
	const auto userpic = _userpicGeometry();
	const auto now = crl::now();
	const auto lift = _liftId
		? _lift.value((_hoveredId == _liftId) ? 1. : 0.)
		: 0.;
	auto hq = PainterHighQualityEnabler(p);
	for (const auto &bubble : _bubbles) {
		const auto hidden = (bubble->id == _sceneId)
			|| (bubble->id == _heldId);
		auto placement = orbitPlacement(*bubble, userpic, progress, now);
		const auto isFront = (placement.depth >= 0.);
		if (isFront != front) {
			continue;
		}
		if (bubble->id == _liftId) {
			placement.radius *= 1. + (kHoverLift - 1.) * lift;
		}
		bubble->drawnCenter = placement.center;
		bubble->drawnRadius = hidden ? 0. : placement.radius;
		bubble->drawnFront = isFront;
		if (hidden || placement.alpha <= 0.) {
			continue;
		}
		const auto extent = std::min(
			kGlowExtent * placement.radius,
			placement.radius + kMaxGlowSpread);
		const auto bounds = QRectF(
			placement.center.x() - extent,
			placement.center.y() - extent,
			2. * extent,
			2. * extent);
		if (!bounds.intersects(QRectF(clip))) {
			continue;
		}
		paintBubble(p, *bubble, placement, false);
	}
	if (_player) {
		_player->markFrameShown();
	}
}

void ProfileGiftBubbles::paintBubble(
		QPainter &p,
		Bubble &bubble,
		const Placement &placement,
		bool large) {
	const auto r = placement.radius;
	if (r < 0.5 || placement.alpha <= 0.) {
		return;
	}
	ensureGlass(bubble);
	const auto useLarge = large
		&& _large
		&& (_large->giftId == bubble.id)
		&& _large->glass.valid();
	const auto &glass = useLarge ? _large->glass : bubble.glass;
	const auto c = placement.center;
	const auto wasOpacity = p.opacity();
	p.setOpacity(wasOpacity * placement.alpha);

	// The glow image is always the small one: it is a soft gradient.
	const auto glowRadius = std::min(kGlowExtent * r, r + kMaxGlowSpread);
	p.drawImage(
		QRectF(
			c.x() - glowRadius,
			c.y() - glowRadius,
			2. * glowRadius,
			2. * glowRadius),
		bubble.glass.glow);

	const auto bodyRadius = kBodyExtent * r;
	const auto body = QRectF(
		c.x() - bodyRadius,
		c.y() - bodyRadius,
		2. * bodyRadius,
		2. * bodyRadius);
	p.drawImage(body, glass.body);

	auto frame = QImage();
	auto artFraction = kArtFraction;
	if (useLarge && _large->player && _large->player->ready()) {
		const auto size = st::creditsHistoryEntryStarGiftSize;
		frame = _large->player->frameInfo({
			.box = QSize(size, size) * style::DevicePixelRatio(),
		}).image;
		_large->player->markFrameShown();
		artFraction = kLargeArtFraction;
	} else if (bubble.animation && bubble.animation->ready()) {
		frame = bubble.animation->frame();
		if (large) {
			artFraction = kLargeArtFraction;
		}
	}
	if (!frame.isNull()) {
		const auto art = r * artFraction;
		const auto was = p.transform();
		p.translate(c);
		if (placement.tilt != 0.) {
			p.rotate(placement.tilt);
		}
		p.drawImage(QRectF(-art / 2., -art / 2., art, art), frame);
		p.setTransform(was);
	}

	p.drawImage(body, glass.highlight);
	p.setOpacity(wasOpacity);
}

void ProfileGiftBubbles::paintHeldLayer(QPainter &p) {
	const auto bubble = _heldId ? findBubble(_heldId) : nullptr;
	if (!bubble || bubble->id == _sceneId) {
		return;
	}
	const auto size = _heldLayer->size();
	if (_heldClipSize != size) {
		// The header is rounded the same way (Mokagram radius 32).
		_heldClipSize = size;
		_heldClip = QPainterPath();
		_heldClip.addRoundedRect(QRectF(QPoint(), size), 32., 32.);
	}
	auto hq = PainterHighQualityEnabler(p);
	p.setClipPath(_heldClip);
	const auto placement = heldPlacement(
		*bubble,
		_userpicGeometry(),
		_progress(),
		crl::now());
	paintBubble(p, *bubble, placement, true);
	if (_player) {
		_player->markFrameShown();
	}
}

int ProfileGiftBubbles::hitTest(QPoint point, bool frontOnly) const {
	if (_sceneId || _heldId || _progress() < 0.999) {
		return -1;
	}
	auto result = -1;
	auto best = 0.;
	for (auto i = 0, count = int(_bubbles.size()); i != count; ++i) {
		const auto &bubble = _bubbles[i];
		const auto r = bubble->drawnRadius;
		if (r <= 0. || (frontOnly && !bubble->drawnFront)) {
			continue;
		}
		const auto d = QPointF(point) - bubble->drawnCenter;
		if (d.x() * d.x() + d.y() * d.y() > r * r) {
			continue;
		}
		// Prefer the near ones, then the bigger ones.
		const auto score = (bubble->drawnFront ? 1000. : 0.) + r;
		if (result < 0 || score > best) {
			result = i;
			best = score;
		}
	}
	return result;
}

bool ProfileGiftBubbles::handleMouse(
		QWidget *source,
		QEvent *e,
		bool frontOnly) {
	const auto type = e->type();
	if (type == QEvent::Leave) {
		if (!_pressedId) {
			setHovered(-1);
		}
		return false;
	} else if (type != QEvent::MouseMove
		&& type != QEvent::MouseButtonPress
		&& type != QEvent::MouseButtonRelease) {
		return false;
	}
	const auto mouse = static_cast<QMouseEvent*>(e);
	const auto point = (source == _header.get())
		? mouse->pos()
		: source->mapTo(_header.get(), mouse->pos());
	const auto now = crl::now();
	if (type == QEvent::MouseMove) {
		if (_pressedId) {
			if (_heldId) {
				_dragPosition = QPointF(point);
				updateHeldLayer();
			} else if ((point - _pressPosition).manhattanLength()
					> kDragThreshold) {
				startHold(QPointF(point), now);
			}
			return true;
		}
		setHovered(hitTest(point, frontOnly));
		return false;
	} else if (type == QEvent::MouseButtonPress) {
		if (mouse->button() != Qt::LeftButton || _pressedId) {
			return false;
		}
		const auto index = hitTest(point, frontOnly);
		if (index < 0) {
			return false;
		}
		_pressedId = _bubbles[index]->id;
		_pressPosition = point;
		ensureAnimating();
		return true;
	}
	if (!_pressedId || mouse->button() != Qt::LeftButton) {
		return false;
	}
	const auto id = std::exchange(_pressedId, uint64(0));
	if (_heldId && !_returnStarted) {
		releaseHold(now);
	} else if (!_heldId) {
		openScene(id);
	}
	return true;
}

void ProfileGiftBubbles::setHovered(int index) {
	const auto id = (index >= 0 && index < int(_bubbles.size()))
		? _bubbles[index]->id
		: uint64(0);
	if (_hoveredId == id) {
		return;
	}
	auto from = _liftId
		? _lift.value((_hoveredId == _liftId) ? 1. : 0.)
		: 0.;
	if (id && _liftId != id) {
		from = 0.;
	}
	_hoveredId = id;
	_header->setCursor(id ? style::cur_pointer : style::cur_default);
	if (id) {
		_liftId = id;
	}
	_lift.start(
		[=] { updateArea(); },
		from,
		id ? 1. : 0.,
		kLiftDuration,
		anim::easeOutCubic);
	ensureAnimating();
	updateArea();
}

void ProfileGiftBubbles::startHold(QPointF position, crl::time now) {
	const auto bubble = findBubble(_pressedId);
	if (!bubble) {
		_pressedId = 0;
		return;
	}
	_hoveredId = 0;
	_liftId = 0;
	_lift.stop();
	_heldId = bubble->id;
	_grabStarted = now;
	_returnStarted = 0;
	_dragPosition = position;
	ensureLarge(*bubble);
	if (!_heldLayer) {
		_heldLayer = base::make_unique_q<HeldLayer>(
			_header.get(),
			[=](QPainter &p) { paintHeldLayer(p); });
		_header->sizeValue() | rpl::on_next([=](QSize size) {
			_heldLayer->setGeometry(QRect(QPoint(), size));
		}, _heldLayer->lifetime());
	}
	_heldLayer->show();
	_heldLayer->raise();
	ensureAnimating();
	updateArea();
}

void ProfileGiftBubbles::releaseHold(crl::time now) {
	const auto bubble = _heldId ? findBubble(_heldId) : nullptr;
	if (!bubble) {
		clearHold();
		return;
	}
	_released = heldPlacement(
		*bubble,
		_userpicGeometry(),
		_progress(),
		now);
	_returnStarted = now;
	if (anim::Disabled()) {
		clearHold();
	}
	ensureAnimating();
	updateArea();
}

void ProfileGiftBubbles::clearHold() {
	_heldId = 0;
	_grabStarted = 0;
	_returnStarted = 0;
	if (_heldLayer) {
		_heldLayer->hide();
	}
	if (!_sceneId) {
		_large = nullptr;
	}
	_header->update();
}

void ProfileGiftBubbles::updateHeldLayer() {
	if (_heldLayer && _heldLayer->isVisible()) {
		_heldLayer->update();
	}
}

bool ProfileGiftBubbles::tick(crl::time now) {
	if (!canAnimate()) {
		_lastTick = 0;
		return false;
	}
	const auto dt = _lastTick
		? std::min(now - _lastTick, crl::time(100))
		: crl::time(0);
	_lastTick = now;
	const auto holding = _pressedId || (_heldId && !_returnStarted);
	if (!_hoveredId && !holding && !anim::Disabled()) {
		_orbitTime += dt;
	}
	if (_heldId
		&& _returnStarted
		&& (now - _returnStarted >= kReturnDuration)) {
		clearHold();
	}

	auto appearing = false;
	for (const auto &bubble : _bubbles) {
		if (now - bubble->shownAt < kAppearDuration) {
			appearing = true;
			break;
		}
	}
	const auto busy = appearing || _lift.animating() || _heldId;
	if (anim::Disabled() && !busy && !_pressedId) {
		// A still picture: nothing to drive (and no repaint, otherwise
		// the paint would start the loop again).
		return false;
	}
	if (busy || (now - _lastUpdate >= kIdleFrame)) {
		_lastUpdate = now;
		updateArea();
	}
	return true;
}

void ProfileGiftBubbles::updateArea() {
	if (_heldId) {
		_header->update();
		updateHeldLayer();
		return;
	}
	const auto userpic = _userpicGeometry();
	const auto k = userpic.width()
		/ float64(st::infoProfileTopBarPhotoSize);
	const auto bottom = int(std::ceil(
		userpic.y() + userpic.height() / 2. + 90. * k)) + 1;
	_header->update(
		0,
		0,
		_header->width(),
		std::min(_header->height(), bottom));
}

bool ProfileGiftBubbles::canAnimate() const {
	if (_bubbles.empty() || !_header->isVisible()) {
		return false;
	}
	const auto window = _header->window();
	if (!window || !window->isVisible() || window->isMinimized()) {
		return false;
	}
	if (const auto handle = window->windowHandle()) {
		const auto visibility = handle->visibility();
		if (visibility == QWindow::Hidden
			|| visibility == QWindow::Minimized) {
			return false;
		}
	}
	return _heldId || _pressedId || (_progress() > 0.5);
}

void ProfileGiftBubbles::ensureAnimating() {
	if (!_animation.animating() && canAnimate()) {
		_lastTick = 0;
		_animation.start();
	}
}

QRectF ProfileGiftBubbles::bubbleRectIn(
		uint64 giftId,
		QWidget *widget) const {
	const auto bubble = findBubble(giftId);
	if (!bubble) {
		return QRectF();
	}
	const auto placement = orbitPlacement(
		*bubble,
		_userpicGeometry(),
		_progress(),
		crl::now());
	const auto r = std::max(placement.radius, 1.);
	const auto origin = QPointF(_header->mapTo(widget, QPoint()));
	return QRectF(
		origin.x() + placement.center.x() - r,
		origin.y() + placement.center.y() - r,
		2. * r,
		2. * r);
}

void ProfileGiftBubbles::openScene(uint64 giftId) {
	const auto bubble = findBubble(giftId);
	if (!bubble || _scene) {
		return;
	}
	// The scene starts from where the grown bubble is drawn right now.
	const auto parent = _header->window();
	const auto start = bubble->drawnRadius;
	const auto startCenter = QPointF(_header->mapTo(parent, QPoint()))
		+ bubble->drawnCenter;
	ensureLarge(*bubble);
	_sceneId = giftId;
	setHovered(-1);

	const auto weak = base::make_weak(this);
	const auto started = crl::now();
	auto callbacks = Scene::Callbacks{
		.paintBubble = [=](
				QPainter &p,
				QPointF center,
				float64 radius,
				float64 alpha,
				float64 tilt) {
			if (const auto strong = weak.get()) {
				strong->paintScene(p, giftId, center, radius, alpha, tilt);
			}
		},
		.origin = [=]() -> QRectF {
			const auto strong = weak.get();
			if (!strong || !strong->_scene) {
				return QRectF();
			}
			// Flies in from the grown bubble, back to the orbit place.
			const auto back = strong->bubbleRectIn(
				giftId,
				strong->_scene->parentWidget());
			const auto mix = std::clamp(
				(crl::now() - started) / float64(kSceneOpenDuration),
				0.,
				1.);
			const auto from = QRectF(
				startCenter.x() - start,
				startCenter.y() - start,
				2. * start,
				2. * start);
			if (back.isEmpty()) {
				return from;
			}
			return QRectF(
				Lerp(from.x(), back.x(), mix),
				Lerp(from.y(), back.y(), mix),
				Lerp(from.width(), back.width(), mix),
				Lerp(from.height(), back.height(), mix));
		},
		.open = [=] {
			if (const auto strong = weak.get()) {
				strong->openGiftBox(giftId);
			}
		},
		.finished = [=] {
			if (const auto strong = weak.get()) {
				strong->finishScene();
			}
		},
	};
	_scene = base::make_unique_q<Scene>(parent, std::move(callbacks));
	updateArea();
}

void ProfileGiftBubbles::paintScene(
		QPainter &p,
		uint64 giftId,
		QPointF center,
		float64 radius,
		float64 alpha,
		float64 tilt) {
	const auto bubble = findBubble(giftId);
	if (!bubble) {
		return;
	}
	auto hq = PainterHighQualityEnabler(p);
	paintBubble(p, *bubble, Placement{
		.center = center,
		.radius = radius,
		.depth = 1.,
		.tilt = tilt,
		.alpha = std::max(alpha, 0.01),
	}, true);
	if (_player) {
		_player->markFrameShown();
	}
}

void ProfileGiftBubbles::finishScene() {
	_scene = nullptr;
	_sceneId = 0;
	if (!_heldId) {
		_large = nullptr;
	}
	updateArea();
	ensureAnimating();
}

void ProfileGiftBubbles::openGiftBox(uint64 giftId) {
	if (const auto bubble = findBubble(giftId)) {
		::Settings::ShowSavedStarGiftBox(_controller, _peer, *bubble->data);
	}
}

} // namespace StaticGram
