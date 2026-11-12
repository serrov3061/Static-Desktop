/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/unique_qptr.h"
#include "base/weak_ptr.h"
#include "ui/effects/animations.h"

class PeerData;

namespace Data {
struct SavedStarGift;
} // namespace Data

namespace Lottie {
class MultiPlayer;
} // namespace Lottie

namespace Ui {
class RpWidget;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace StaticGram {

// Pinned collectible gifts of a profile fly around the avatar in glass
// bubbles (a port of the Android ProfileGiftsView / ProfileGiftScene).
//
// The profile header (Info::Profile::TopBar) owns one instance, feeds it the
// pinned gifts and calls paintBehind() / paintFront() around the avatar.
// Hover lifts a bubble a little; press and drag pulls it out of the orbit,
// it inflates with the distance from the avatar and flies back on release.
// It always stays inside the header: the header never changes its size.
// A click opens a full-window scene with the gift in the middle and an
// "Open" button for the unique gift box.
//
// Frames are only driven while the header is visible and the window is not
// minimized; the glass layers are rendered once per gift, size and DPR.
class ProfileGiftBubbles final : public base::has_weak_ptr {
public:
	struct Descriptor {
		not_null<Ui::RpWidget*> header;
		not_null<Window::SessionController*> controller;
		not_null<PeerData*> peer;
		QWidget *userpicButton = nullptr;
		Fn<QRect()> userpicGeometry;
		Fn<float64()> progress; // 0. collapsed .. 1. expanded header.
		Fn<int()> contentTop; // Held bubbles stay below it (top bar).
		Fn<int()> contentBottom; // And above it (action buttons).
	};

	explicit ProfileGiftBubbles(Descriptor &&descriptor);
	~ProfileGiftBubbles();

	// Unique gifts only, the emoji status one already filtered out.
	void setGifts(const std::vector<Data::SavedStarGift> &gifts);
	[[nodiscard]] bool empty() const;

	// The far half of the orbit, painted before the avatar.
	void paintBehind(QPainter &p, const QRect &clip);
	// The near half, painted after the avatar.
	void paintFront(QPainter &p, const QRect &clip);

	struct Bubble;
	struct Placement {
		QPointF center;
		float64 radius = 0.;
		float64 depth = 0.;
		float64 tilt = 0.;
		float64 alpha = 0.;
	};

private:
	class HeldLayer;
	class Scene;
	struct GlassImages;

	void paintOrbit(QPainter &p, const QRect &clip, bool front);
	void paintBubble(
		QPainter &p,
		Bubble &bubble,
		const Placement &placement,
		bool large);
	void paintHeldLayer(QPainter &p);
	void paintScene(
		QPainter &p,
		uint64 giftId,
		QPointF center,
		float64 radius,
		float64 alpha,
		float64 tilt);

	[[nodiscard]] Placement orbitPlacement(
		const Bubble &bubble,
		const QRect &userpic,
		float64 progress,
		crl::time now) const;
	[[nodiscard]] Placement heldPlacement(
		const Bubble &bubble,
		const QRect &userpic,
		float64 progress,
		crl::time now) const;
	[[nodiscard]] Bubble *findBubble(uint64 giftId) const;
	[[nodiscard]] int hitTest(QPoint point, bool frontOnly) const;

	void setupLottie(Bubble &bubble);
	void checkLoaded();
	void ensureGlass(Bubble &bubble);
	void ensureLarge(Bubble &bubble);

	[[nodiscard]] bool canAnimate() const;
	void ensureAnimating();
	bool tick(crl::time now);
	void updateArea();

	bool handleMouse(QWidget *source, QEvent *e, bool frontOnly);
	void setHovered(int index);
	void startHold(QPointF position, crl::time now);
	void releaseHold(crl::time now);
	void clearHold();
	void updateHeldLayer();
	void openScene(uint64 giftId);
	void finishScene();
	void openGiftBox(uint64 giftId);
	[[nodiscard]] QRectF bubbleRectIn(uint64 giftId, QWidget *widget) const;

	const not_null<Ui::RpWidget*> _header;
	const not_null<Window::SessionController*> _controller;
	const not_null<PeerData*> _peer;
	const Fn<QRect()> _userpicGeometry;
	const Fn<float64()> _progress;
	const Fn<int()> _contentTop;
	const Fn<int()> _contentBottom;

	std::vector<std::unique_ptr<Bubble>> _bubbles;
	std::unique_ptr<Lottie::MultiPlayer> _player;
	rpl::lifetime _loadingLifetime;

	// The large artwork and glass of the held / opened gift.
	struct Large;
	std::unique_ptr<Large> _large;

	Ui::Animations::Basic _animation;
	crl::time _lastTick = 0;
	crl::time _lastUpdate = 0;
	crl::time _orbitTime = 0;

	// Hover: a small lift of the bubble under the cursor.
	uint64 _hoveredId = 0;
	uint64 _liftId = 0;
	Ui::Animations::Simple _lift;

	// Press: a click opens the scene, a drag pulls the bubble out.
	uint64 _pressedId = 0;
	QPoint _pressPosition;
	uint64 _heldId = 0;
	QPointF _dragPosition;
	crl::time _grabStarted = 0;
	crl::time _returnStarted = 0;
	Placement _released;
	base::unique_qptr<HeldLayer> _heldLayer;
	QPainterPath _heldClip;
	QSize _heldClipSize;

	base::unique_qptr<Scene> _scene;
	uint64 _sceneId = 0;

	rpl::lifetime _lifetime;

};

} // namespace StaticGram
