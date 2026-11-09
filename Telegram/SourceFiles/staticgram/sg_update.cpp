/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_update.h"

#include "base/call_delayed.h"
#include "core/application.h"
#include "core/update_checker.h"
#include "lang/lang_keys.h"
#include "ui/basic_click_handlers.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "ui/vertical_list.h"
#include "window/window_controller.h"
#include "styles/style_layers.h"

#include <QtCore/QPointer>

namespace StaticGram {
namespace {

constexpr auto kRetryShowDelay = crl::time(1000);

enum class Phase {
	Checking,
	Downloading,
	Ready,
	Failed,
	Manual,
};

struct State {
	Phase phase = Phase::Checking;
	int percent = 0;
};

QPointer<Ui::BoxContent> ShownBox;
Core::ForcedUpdate Current;

[[nodiscard]] State InitialState() {
	if (!Core::UpdateSelfInstallAvailable()) {
		return { Phase::Manual };
	}
	using Checker = Core::UpdateChecker;
	const auto checker = Checker();
	switch (checker.state()) {
	case Checker::State::Ready: return { Phase::Ready };
	case Checker::State::Download: return { Phase::Downloading };
	case Checker::State::None: break;
	}
	return { Phase::Checking };
}

[[nodiscard]] rpl::producer<State> StateValue() {
	if (!Core::UpdateSelfInstallAvailable()) {
		return rpl::single(State{ Phase::Manual });
	}
	const auto checker = Core::UpdateChecker();
	using Progress = Core::UpdateChecker::Progress;
	return rpl::single(
		InitialState()
	) | rpl::then(rpl::merge(
		checker.checking() | rpl::map([] {
			return State{ Phase::Checking };
		}),
		checker.isLatest() | rpl::map([] {
			return State{ Phase::Checking };
		}),
		checker.progress() | rpl::map([](Progress progress) {
			const auto percent = (progress.size > 0)
				? int(std::clamp(
					progress.already * 100 / progress.size,
					int64(0),
					int64(100)))
				: 0;
			return State{ Phase::Downloading, percent };
		}),
		checker.failed() | rpl::map([] {
			return State{ Phase::Failed };
		}),
		checker.ready() | rpl::map([] {
			return State{ Phase::Ready };
		})));
}

void OpenDownloadPage() {
	UrlClickHandler::Open(Core::UpdateManualDownloadUrl());
}

void RetryNow() {
	cSetLastUpdateCheck(0);
	Core::UpdateChecker().start();
}

void ForcedUpdateBox(
		not_null<Ui::GenericBox*> box,
		const Core::ForcedUpdate &update) {
	box->setTitle(tr::lng_sg_update_title());
	box->setCloseByEscape(false);
	box->setCloseByOutsideClick(false);

	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_sg_update_forced_text(
			lt_version,
			rpl::single(update.versionName)),
		st::boxLabel));
	Ui::AddSkip(box->verticalLayout());

	const auto state = box->lifetime().make_state<rpl::variable<State>>(
		StateValue());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		state->value() | rpl::map([](State state) -> rpl::producer<QString> {
			switch (state.phase) {
			case Phase::Checking: return tr::lng_sg_update_checking();
			case Phase::Downloading: return tr::lng_sg_update_downloading(
				lt_percent,
				rpl::single(QString::number(state.percent) + '%'));
			case Phase::Ready: return tr::lng_sg_update_ready();
			case Phase::Failed: return tr::lng_sg_update_failed();
			case Phase::Manual: return tr::lng_sg_update_manual();
			}
			Unexpected("Phase in ForcedUpdateBox.");
		}) | rpl::flatten_latest(),
		st::boxDividerLabel));

	state->value(
	) | rpl::map([](State state) {
		return state.phase;
	}) | rpl::distinct_until_changed(
	) | rpl::on_next([=](Phase phase) {
		box->clearButtons();
		switch (phase) {
		case Phase::Ready:
			box->addButton(tr::lng_sg_update_restart(), [] {
				Core::checkReadyUpdate();
				Core::Restart();
			});
			box->addLeftButton(
				tr::lng_sg_update_download(),
				OpenDownloadPage);
			break;
		case Phase::Failed:
			box->addButton(tr::lng_sg_update_retry(), RetryNow);
			box->addLeftButton(
				tr::lng_sg_update_download(),
				OpenDownloadPage);
			break;
		case Phase::Manual:
		case Phase::Checking:
		case Phase::Downloading:
			box->addButton(tr::lng_sg_update_download(), OpenDownloadPage);
			break;
		}
	}, box->lifetime());
}

void ShowForcedBox() {
	if (!Current || ShownBox || Core::Quitting()) {
		return;
	}
	const auto window = Core::IsAppLaunched()
		? Core::App().activePrimaryWindow()
		: nullptr;
	if (!window) {
		base::call_delayed(kRetryShowDelay, ShowForcedBox);
		return;
	}
	const auto box = window->show(
		Box(ForcedUpdateBox, Current),
		Ui::LayerOption::KeepOther);
	ShownBox = box.get();
	if (const auto raw = ShownBox.data()) {
		// Something else may close the layer (another box with
		// CloseOther, a logout), the box comes back while required.
		raw->boxClosing() | rpl::on_next([] {
			ShownBox = nullptr;
			if (Current && !Core::Quitting()) {
				base::call_delayed(kRetryShowDelay, ShowForcedBox);
			}
		}, raw->lifetime());
	}
}

} // namespace

void SetupForcedUpdate(rpl::lifetime &lifetime) {
	if (Core::UpdaterDisabled()) {
		return;
	}
	Core::ForcedUpdateValue(
	) | rpl::on_next([](Core::ForcedUpdate update) {
		const auto changed = (Current.version != update.version);
		Current = update;
		if (!Current) {
			if (const auto box = ShownBox.data()) {
				ShownBox = nullptr;
				box->closeBox();
			}
		} else if (changed && ShownBox) {
			// A newer mandatory release: show its version.
			const auto box = ShownBox.data();
			ShownBox = nullptr;
			box->closeBox();
		} else {
			ShowForcedBox();
		}
	}, lifetime);
}

} // namespace StaticGram
