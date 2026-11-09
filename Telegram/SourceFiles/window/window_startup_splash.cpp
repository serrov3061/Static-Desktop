/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "window/window_startup_splash.h"

#include "base/event_filter.h"
#include "base/timer.h"
#include "lottie/lottie_icon.h"
#include "ui/effects/animations.h"
#include "ui/rp_widget.h"
#include "window/themes/window_theme.h"
#include "styles/palette.h"

#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>

namespace Window {
namespace {

// Lottie canvas is 1320x1320 with the crystal taking 440 px of it,
// so a 336 px canvas gives a ~112 px crystal (same as on Android).
constexpr auto kLogoSize = 336;
constexpr auto kFramesPerSecond = 60;
constexpr auto kHideDuration = crl::time(380);
constexpr auto kHideScale = 1.35;
constexpr auto kFallbackTimeout = crl::time(2600);

bool SplashShown = false;

class StartupSplash final : public Ui::RpWidget {
public:
	explicit StartupSplash(not_null<Ui::RpWidget*> parent);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	void startHide();

	const QColor _bg;
	std::unique_ptr<Lottie::Icon> _icon;
	Ui::Animations::Simple _hiding;
	base::Timer _finishTimer;
	base::Timer _fallbackTimer;
	bool _hideStarted = false;

};

[[nodiscard]] QColor SplashBackground() {
	auto result = st::windowBg->c;
	result.setAlpha(255);
	return result;
}

StartupSplash::StartupSplash(not_null<Ui::RpWidget*> parent)
: RpWidget(parent.get())
, _bg(SplashBackground())
, _finishTimer([=] { startHide(); })
, _fallbackTimer([=] { startHide(); }) {
	const auto dark = Window::Theme::IsNightMode()
		|| (_bg.lightness() < 128);
	const auto size = style::ConvertScale(kLogoSize);
	_icon = Lottie::MakeIcon({
		.path = (dark
			? u":/animations/splash_dark.tgs"_q
			: u":/animations/splash_light.tgs"_q),
		.sizeOverride = QSize(size, size),
	});

	parent->sizeValue(
	) | rpl::on_next([=](QSize outer) {
		setGeometry(QRect(QPoint(), outer));
	}, lifetime());

	// Stay above widgets that are added to the body while we are shown.
	base::install_event_filter(this, parent, [=](not_null<QEvent*> e) {
		if (e->type() == QEvent::ChildAdded) {
			crl::on_main(this, [=] {
				raise();
			});
		}
		return base::EventFilterResult::Continue;
	});

	show();
	raise();

	if (!_icon || !_icon->valid() || _icon->framesCount() <= 0) {
		_icon = nullptr;
		startHide();
		return;
	}
	const auto frames = _icon->framesCount();
	const auto duration = crl::time(frames * 1000 / kFramesPerSecond);
	_icon->animate([=] { update(); }, 0, frames - 1, duration);
	_finishTimer.callOnce(duration);
	_fallbackTimer.callOnce(kFallbackTimeout);
}

void StartupSplash::startHide() {
	if (_hideStarted) {
		return;
	}
	_hideStarted = true;
	_finishTimer.cancel();
	_fallbackTimer.cancel();
	setAttribute(Qt::WA_TransparentForMouseEvents);
	_hiding.start([=] {
		update();
		if (!_hiding.animating()) {
			hide();
			deleteLater();
		}
	}, 0., 1., kHideDuration);
}

void StartupSplash::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);

	const auto progress = _hiding.value(_hideStarted ? 1. : 0.);
	const auto eased = progress * progress; // Ease-in.
	const auto opacity = 1. - eased;
	if (opacity <= 0.) {
		return;
	}
	p.setOpacity(opacity);
	p.fillRect(e->rect(), _bg);

	if (!_icon) {
		return;
	}
	const auto scale = 1. + (kHideScale - 1.) * eased;
	const auto size = _icon->size();
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	p.translate(width() / 2., height() / 2.);
	p.scale(scale, scale);
	_icon->paint(p, -size.width() / 2, -size.height() / 2);
}

} // namespace

void ShowStartupSplash(not_null<Ui::RpWidget*> body) {
	if (SplashShown) {
		return;
	}
	SplashShown = true;
	new StartupSplash(body); // Owned by the body widget.
}

} // namespace Window
