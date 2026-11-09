/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_money.h"

#include "api/api_credits.h"
#include "apiwrap.h"
#include "base/call_delayed.h"
#include "data/data_credits.h"
#include "data/data_session.h"
#include "data/data_star_gift.h"
#include "data/data_user.h"
#include "info/peer_gifts/info_peer_gifts_common.h"
#include "info/statistics/info_statistics_list_controllers.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mokagram/ui/mokagram_box_parts.h"
#include "settings/settings_credits_graphics.h" // ReceiptCreditsBox.
#include "staticgram/sg_money_cards.h"
#include "staticgram/sg_money_editor.h"
#include "staticgram/sg_money_nft.h"
#include "staticgram/sg_money_send.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/discrete_sliders.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_credits.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_staticgram.h"
#include "styles/style_widgets.h"

namespace StaticGram::Money {
namespace {

constexpr auto kGiftsPerRow = 3;

[[nodiscard]] QString UnitName(Kind kind) {
	return (kind == Kind::Gram) ? u"GRAM"_q : u"Stars"_q;
}

not_null<Ui::FlatLabel*> AddCenteredLabel(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		const style::FlatLabel &st,
		const style::margins &padding) {
	return container->add(
		object_ptr<Ui::FlatLabel>(container, std::move(text), st),
		padding);
}

void AddEditCardLink(
		not_null<Ui::VerticalLayout*> container,
		not_null<Window::SessionController*> controller,
		Fn<Kind()> kind) {
	const auto wrap = container->add(
		object_ptr<Ui::RpWidget>(container),
		st::sgMoneyLinkPadding);
	const auto link = Ui::CreateChild<Ui::LinkButton>(
		wrap,
		tr::lng_sg_money_edit_card(tr::now));
	link->setClickedCallback([=] {
		ShowCardEditor(controller, kind());
	});
	rpl::combine(
		wrap->widthValue(),
		link->sizeValue()
	) | rpl::on_next([=](int width, QSize size) {
		wrap->resize(width, size.height());
		link->moveToLeft((width - size.width()) / 2, 0, width);
	}, wrap->lifetime());
}

not_null<Ui::RoundButton*> AddSendButton(
		not_null<Ui::VerticalLayout*> container,
		not_null<CardsPager*> pager) {
	const auto button = container->add(
		object_ptr<Ui::RoundButton>(
			container,
			pager->kindValue() | rpl::map([](Kind kind) {
				return (kind == Kind::Gram)
					? tr::lng_sg_money_send_gram(tr::now)
					: tr::lng_sg_money_send_stars(tr::now);
			}),
			Mokagram::BoxParts::PillActive()),
		st::sgMoneyButtonPadding);
	button->setTextTransform(Ui::RoundButtonTextTransform::NoTransform);
	container->widthValue() | rpl::on_next([=](int width) {
		const auto &padding = st::sgMoneyButtonPadding;
		button->setFullWidth(width - padding.left() - padding.right());
	}, button->lifetime());
	return button;
}

// The upstream credits history list (rows, paging, receipts), asked for
// with the ton flag for GRAM.
void FillTransactions(
		not_null<Ui::VerticalLayout*> container,
		not_null<Window::SessionController*> controller,
		Kind kind) {
	const auto self = controller->session().user();
	const auto ton = (kind == Kind::Gram);
	const auto placeholder = container->add(
		object_ptr<Ui::VerticalLayout>(container));
	AddCenteredLabel(
		placeholder,
		tr::lng_sg_money_loading(),
		st::sgMoneyEmptyLabel,
		st::sgMoneyEmptyPadding);

	const auto api = container->lifetime().make_state<Api::CreditsHistory>(
		self,
		true,
		true,
		ton);
	const auto weak = base::make_weak(controller);
	api->request({}, crl::guard(container, [=](
			Data::CreditsStatusSlice slice) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		placeholder->clear();
		if (slice.list.empty()) {
			AddCenteredLabel(
				placeholder,
				tr::lng_sg_money_no_transactions(),
				st::sgMoneyEmptyLabel,
				st::sgMoneyEmptyPadding);
		} else {
			const auto clicked = [=](
					const Data::CreditsHistoryEntry &e,
					const Data::SubscriptionEntry &s) {
				if (const auto strong = weak.get()) {
					strong->uiShow()->show(Box(
						::Settings::ReceiptCreditsBox,
						strong,
						e,
						s));
				}
			};
			Info::Statistics::AddCreditsHistoryList(
				strong->uiShow(),
				slice,
				container,
				clicked,
				self,
				true,
				true,
				false,
				ton);
		}
		container->resizeToWidth(container->width());
	}));
}

class GiftsGrid final : public Ui::RpWidget {
public:
	GiftsGrid(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		Fn<void(const NftItem&)> clicked);

	void append(std::vector<NftItem> items);
	void clear();
	[[nodiscard]] bool empty() const;

protected:
	int resizeGetHeight(int newWidth) override;

private:
	void layout(int width);

	const Fn<void(const NftItem&)> _clicked;
	Info::PeerGifts::Delegate _delegate;
	std::vector<std::unique_ptr<Info::PeerGifts::GiftButton>> _buttons;

};

GiftsGrid::GiftsGrid(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	Fn<void(const NftItem&)> clicked)
: RpWidget(parent)
, _clicked(std::move(clicked))
, _delegate(
	&controller->session(),
	Info::PeerGifts::GiftButtonMode::Minimal) {
}

bool GiftsGrid::empty() const {
	return _buttons.empty();
}

void GiftsGrid::clear() {
	_buttons.clear();
	resizeToWidth(width());
}

void GiftsGrid::append(std::vector<NftItem> items) {
	for (auto &item : items) {
		if (!item.gift) {
			continue;
		}
		auto button = std::make_unique<Info::PeerGifts::GiftButton>(
			this,
			&_delegate);
		const auto raw = button.get();
		raw->setDescriptor(
			Info::PeerGifts::GiftTypeStars{
				.info = *item.gift,
				.mine = true,
			},
			Info::PeerGifts::GiftButtonMode::Minimal);
		raw->setClickedCallback([this, item = std::move(item)] {
			_clicked(item);
		});
		raw->show();
		_buttons.push_back(std::move(button));
	}
	resizeToWidth(width());
}

int GiftsGrid::resizeGetHeight(int newWidth) {
	layout(newWidth);
	if (_buttons.empty()) {
		return 0;
	}
	const auto &padding = st::sgMoneyGiftsPadding;
	const auto single = _delegate.buttonSize();
	const auto rows = (int(_buttons.size()) + kGiftsPerRow - 1)
		/ kGiftsPerRow;
	return padding.top()
		+ rows * single.height()
		+ (rows - 1) * st::sgMoneyGiftsSkip
		+ padding.bottom();
}

void GiftsGrid::layout(int width) {
	const auto &padding = st::sgMoneyGiftsPadding;
	const auto skip = st::sgMoneyGiftsSkip;
	const auto available = width - padding.left() - padding.right();
	const auto single = QSize(
		std::max((available - (kGiftsPerRow - 1) * skip) / kGiftsPerRow, 1),
		_delegate.buttonSize().height());
	const auto extend = _delegate.buttonExtend();
	for (auto i = 0, count = int(_buttons.size()); i != count; ++i) {
		const auto row = i / kGiftsPerRow;
		const auto column = i % kGiftsPerRow;
		_buttons[i]->setGeometry(
			QRect(
				QPoint(
					padding.left() + column * (single.width() + skip),
					padding.top() + row * (single.height() + skip)),
				single),
			extend);
	}
}

// wallet.getNfts: the collectibles of the custodial chain wallet. A click
// opens the actions (Android SGNftSheet / StarGiftSheet in wallet mode).
void FillCollectibles(
		not_null<Ui::VerticalLayout*> container,
		not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	const auto weak = base::make_weak(controller);
	const auto placeholder = container->add(
		object_ptr<Ui::VerticalLayout>(container));
	AddCenteredLabel(
		placeholder,
		tr::lng_sg_money_loading(),
		st::sgMoneyEmptyLabel,
		st::sgMoneyEmptyPadding);

	struct State {
		QString next;
		bool loading = false;
		int generation = 0;
		base::flat_set<QString> removed;
		base::unique_qptr<Ui::PopupMenu> menu;
		Fn<void()> reload;
	};
	const auto state = container->lifetime().make_state<State>();
	const auto moved = [=](const QString &address) {
		return crl::guard(container, [=] {
			// Chain moves land within seconds: hide the item now and
			// reload a few times (Android SGChainWallet.invalidateSoon).
			state->removed.emplace(address);
			state->reload();
			for (const auto delay : { 3000, 8000, 15000 }) {
				base::call_delayed(crl::time(delay), container, [=] {
					state->reload();
				});
			}
		});
	};
	const auto showMenu = [=](const NftItem &item) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		state->menu = base::make_unique_q<Ui::PopupMenu>(
			container,
			st::popupMenuWithIcons);
		FillNftMenu(state->menu.get(), strong, item, moved(item.address));
		state->menu->popup(QCursor::pos());
	};
	const auto grid = container->add(
		object_ptr<GiftsGrid>(container, controller, showMenu));
	// Usernames, numbers and domains: plain rows under the gifts.
	const auto others = container->add(
		object_ptr<Ui::VerticalLayout>(container));
	const auto more = container->add(
		object_ptr<Ui::SlideWrap<Ui::SettingsButton>>(
			container,
			object_ptr<Ui::SettingsButton>(
				container,
				tr::lng_stories_show_more(),
				st::settingsButtonNoIcon)));
	more->hide(anim::type::instant);

	const auto empty = [=] {
		return grid->empty() && !others->count();
	};
	const auto load = std::make_shared<Fn<void(bool)>>();
	*load = [=](bool reset) {
		if (state->loading && !reset) {
			return;
		}
		if (reset) {
			state->next = QString();
		}
		state->loading = true;
		const auto generation = ++state->generation;
		LoadNfts(session, state->next, crl::guard(container, [=](
				NftSlice slice) {
			if (generation != state->generation) {
				return;
			}
			state->loading = false;
			state->next = slice.next;
			if (reset) {
				grid->clear();
				others->clear();
			}
			auto items = std::vector<NftItem>();
			for (auto &item : slice.items) {
				if (state->removed.contains(item.address)) {
					continue;
				} else if (!item.gift) {
					const auto button = others->add(
						object_ptr<Ui::SettingsButton>(
							others,
							rpl::single(item.name.isEmpty()
								? item.address
								: item.name),
							st::settingsButtonNoIcon));
					button->setClickedCallback([=] {
						showMenu(item);
					});
					continue;
				}
				items.push_back(std::move(item));
			}
			grid->append(std::move(items));
			placeholder->clear();
			if (empty()) {
				AddCenteredLabel(
					placeholder,
					tr::lng_sg_money_no_collectibles(),
					st::sgMoneyEmptyLabel,
					st::sgMoneyEmptyPadding);
			}
			more->toggle(!state->next.isEmpty(), anim::type::instant);
			container->resizeToWidth(container->width());
		}), crl::guard(container, [=] {
			if (generation != state->generation) {
				return;
			}
			state->loading = false;
			if (empty()) {
				placeholder->clear();
				AddCenteredLabel(
					placeholder,
					tr::lng_sg_money_load_failed(),
					st::sgMoneyEmptyLabel,
					st::sgMoneyEmptyPadding);
				container->resizeToWidth(container->width());
			}
		}));
	};
	state->reload = [=] { (*load)(true); };
	more->entity()->setClickedCallback([=] { (*load)(false); });
	container->lifetime().add([=] { *load = nullptr; });
	(*load)(false);
}

struct Balances {
	std::optional<int64> gram;
	std::optional<int64> stars;
};

// Cards from sgcard.getCards, own balances kept live on top of them.
void BindCards(
		not_null<CardsPager*> pager,
		not_null<UserData*> user) {
	const auto session = &user->session();
	const auto self = user->isSelf();
	const auto balances = pager->lifetime().make_state<Balances>();
	const auto applyLive = [=] {
		if (balances->gram) {
			pager->setBalance(Kind::Gram, *balances->gram);
		}
		if (balances->stars) {
			pager->setBalance(Kind::Stars, *balances->stars);
		}
	};
	CardsValue(user) | rpl::on_next([=](const Cards &cards) {
		pager->setCards(cards);
		applyLive();
	}, pager->lifetime());
	if (self) {
		GramBalanceValue(session) | rpl::on_next([=](int64 value) {
			balances->gram = value;
			applyLive();
		}, pager->lifetime());
		StarsBalanceValue(session) | rpl::on_next([=](int64 value) {
			balances->stars = value;
			applyLive();
		}, pager->lifetime());
	}
}

void MoneyBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		Kind initial) {
	using namespace Mokagram::BoxParts;
	const auto session = &controller->session();
	const auto self = session->user();

	box->setStyle(PillButtonsBox());
	RoundLayer(box);
	box->setWidth(st::boxWideWidth);
	box->setTitle(tr::lng_sg_money_title());
	box->addTopButton(st::boxTitleClose, [=] { box->closeBox(); });

	const auto container = box->verticalLayout();
	const auto pager = container->add(
		object_ptr<CardsPager>(container, self, initial));
	BindCards(pager, self);
	RefreshBalances(session);

	AddSendButton(container, pager)->setClickedCallback([=] {
		ShowSendMoney(controller, pager->kind());
	});
	AddEditCardLink(container, controller, [=] { return pager->kind(); });

	const auto tabs = container->add(
		object_ptr<Ui::SettingsSlider>(container, st::defaultTabsSlider),
		st::sgMoneyTabsPadding);
	tabs->setSections({
		tr::lng_sg_money_transactions(tr::now),
		tr::lng_sg_money_collectibles(tr::now),
	});
	const auto body = container->add(
		object_ptr<Ui::VerticalLayout>(container));

	struct Shown {
		int tab = -1;
		Kind kind = Kind::Gram;
	};
	const auto shown = box->lifetime().make_state<Shown>();
	rpl::combine(
		rpl::single(0) | rpl::then(tabs->sectionActivated()),
		pager->kindValue()
	) | rpl::on_next([=](int tab, Kind kind) {
		if (shown->tab == tab && (tab != 0 || shown->kind == kind)) {
			return;
		}
		shown->tab = tab;
		shown->kind = kind;
		body->clear();
		const auto inner = body->add(object_ptr<Ui::VerticalLayout>(body));
		if (tab == 0) {
			FillTransactions(inner, controller, kind);
		} else {
			FillCollectibles(inner, controller);
		}
		body->resizeToWidth(container->width());
	}, body->lifetime());
}

void CardSheetBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user,
		Kind initial) {
	using namespace Mokagram::BoxParts;
	const auto self = user->isSelf();

	box->setStyle(PillButtonsBox());
	RoundLayer(box);
	box->setWidth(st::boxWideWidth);
	box->setTitle(rpl::single(user->name()));
	box->addTopButton(st::boxTitleClose, [=] { box->closeBox(); });

	const auto container = box->verticalLayout();
	const auto pager = container->add(
		object_ptr<CardsPager>(container, user, initial));
	BindCards(pager, user);
	if (self) {
		RefreshBalances(&user->session());
	}

	// Android SGCardSheet: «{user} — на счету X.» under the cards.
	const auto changes = pager->lifetime().make_state<rpl::event_stream<>>();
	auto owns = rpl::combine(
		pager->kindValue(),
		CardsValue(user),
		rpl::single(rpl::empty) | rpl::then(changes->events())
	) | rpl::map([=](Kind kind, const Cards &, auto) {
		const auto balance = pager->balance(kind);
		const auto name = user->name();
		return (balance > 0)
			? tr::lng_sg_money_owns(
				tr::now,
				lt_user,
				name,
				lt_amount,
				FormatAmount(kind, balance) + ' ' + UnitName(kind))
			: (kind == Kind::Gram)
			? tr::lng_sg_money_owns_no_gram(tr::now, lt_user, name)
			: tr::lng_sg_money_owns_no_stars(tr::now, lt_user, name);
	});
	AddCenteredLabel(
		container,
		std::move(owns),
		st::sgMoneyAboutLabel,
		st::sgMoneyAboutPadding);
	if (self) {
		const auto session = &user->session();
		rpl::merge(
			GramBalanceValue(session) | rpl::to_empty,
			StarsBalanceValue(session) | rpl::to_empty
		) | rpl::on_next([=] {
			changes->fire({});
		}, pager->lifetime());
	}

	if (self) {
		AddEditCardLink(container, controller, [=] {
			return pager->kind();
		});
		AddCenteredLabel(
			container,
			tr::lng_sg_money_public_note(),
			st::sgMoneyEmptyLabel,
			st::sgMoneyAboutPadding);
		box->addButton(tr::lng_sg_money_title(), [=] {
			box->closeBox();
			ShowMoneyBox(controller, pager->kind());
		}, PillActive());
	} else if (CanReceiveMoney(user)) {
		box->addButton(
			pager->kindValue() | rpl::map([](Kind kind) {
				return (kind == Kind::Gram)
					? tr::lng_sg_money_send_gram(tr::now)
					: tr::lng_sg_money_send_stars(tr::now);
			}),
			[=] { ShowSendMoney(controller, pager->kind(), user); },
			PillActive());
	}
}

} // namespace

void ShowMoneyBox(
		not_null<Window::SessionController*> controller,
		Kind initial) {
	controller->show(Box(MoneyBox, controller, initial));
}

void ShowCardSheet(
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user,
		Kind kind) {
	controller->show(Box(CardSheetBox, controller, user, kind));
}

void ShowCardSheetById(
		not_null<Window::SessionController*> controller,
		uint64 userId,
		Kind kind) {
	const auto session = &controller->session();
	const auto id = UserId(userId);
	if (const auto user = session->data().userLoaded(id)) {
		ShowCardSheet(controller, user, kind);
		return;
	}
	const auto weak = base::make_weak(controller);
	session->api().request(MTPusers_GetUsers(
		MTP_vector<MTPInputUser>(
			1,
			MTP_inputUser(MTP_long(userId), MTP_long(0)))
	)).done([=](const MTPVector<MTPUser> &result) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		strong->session().data().processUsers(result);
		if (const auto user = strong->session().data().userLoaded(id)) {
			ShowCardSheet(strong, user, kind);
		} else {
			strong->showToast(tr::lng_sg_money_user_not_found(tr::now));
		}
	}).fail([=] {
		if (const auto strong = weak.get()) {
			strong->showToast(tr::lng_sg_money_user_not_found(tr::now));
		}
	}).send();
}

} // namespace StaticGram::Money
