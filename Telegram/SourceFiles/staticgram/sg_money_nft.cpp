/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_money_nft.h"

#include "apiwrap.h"
#include "boxes/peer_list_controllers.h"
#include "core/local_url_handlers.h" // ResolveAndShowUniqueGift.
#include "data/data_star_gift.h"
#include "data/data_thread.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/session/session_show.h"
#include "mokagram/ui/mokagram_box_parts.h"
#include "staticgram/sg_money_send.h" // CanReceiveMoney.
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_staticgram.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

namespace StaticGram::Money {
namespace {

struct MoveState {
	bool busy = false;
	QString requestKey = NewNftRequestKey();
};

[[nodiscard]] QString ItemName(const NftItem &item) {
	if (item.gift && item.gift->unique) {
		return Data::UniqueGiftName(*item.gift->unique);
	}
	return item.name.isEmpty()
		? tr::lng_sg_nft_collectible(tr::now)
		: item.name;
}

void ShowMoveError(
		base::weak_ptr<Window::SessionController> weak,
		const QString &type) {
	const auto strong = weak.get();
	if (!strong) {
		return;
	} else if (type == u"FROZEN_METHOD_INVALID"_q
		&& strong->uiShow()->showFrozenError()) {
		return;
	}
	strong->showToast(NftErrorText(type));
}

void Moved(
		base::weak_ptr<Window::SessionController> weak,
		const QString &text,
		Fn<void()> moved) {
	if (const auto strong = weak.get()) {
		strong->showToast(text);
	}
	if (moved) {
		moved();
	}
}

void ConfirmTransferToUser(
		not_null<Window::SessionController*> controller,
		const NftItem &item,
		not_null<UserData*> user,
		Fn<void()> moved) {
	const auto session = &controller->session();
	const auto weak = base::make_weak(controller);
	const auto state = std::make_shared<MoveState>();
	const auto address = item.address;
	controller->show(Ui::MakeConfirmBox({
		.text = tr::lng_sg_nft_transfer_to_user_text(
			tr::now,
			lt_name,
			tr::bold(ItemName(item)),
			lt_user,
			tr::bold(user->firstName.isEmpty()
				? user->name()
				: user->firstName),
			tr::marked),
		.confirmed = [=](Fn<void()> close) {
			if (state->busy) {
				return;
			}
			state->busy = true;
			TransferNft(session, {
				.nftAddress = address,
				.toUser = user,
				.requestKey = state->requestKey,
			}, [=] {
				close();
				Moved(weak, tr::lng_sg_nft_sent(tr::now), moved);
			}, [=](const QString &type) {
				state->busy = false;
				ShowMoveError(weak, type);
			});
		},
		.confirmText = tr::lng_sg_nft_transfer(),
		.title = tr::lng_sg_nft_transfer_title(),
	}), Ui::LayerOption::KeepOther);
}

void ChooseUser(
		not_null<Window::SessionController*> controller,
		const NftItem &item,
		Fn<void()> moved) {
	const auto session = &controller->session();
	const auto chooser = std::make_shared<QPointer<PeerListBox>>();
	const auto weak = base::make_weak(controller);
	auto callback = [=](not_null<Data::Thread*> thread) {
		const auto user = thread->peer()->asUser();
		const auto strong = weak.get();
		if (!strong || !user) {
			return;
		} else if (user->isSelf() || !CanReceiveMoney(user)) {
			strong->showToast(tr::lng_sg_nft_only_users(tr::now));
			return;
		}
		if (const auto box = chooser->data()) {
			box->closeBox();
		}
		ConfirmTransferToUser(strong, item, not_null(user), moved);
	};
	auto filter = [](not_null<Data::Thread*> thread) {
		const auto user = thread->asHistory()
			? thread->peer()->asUser()
			: nullptr;
		return user && !user->isSelf() && CanReceiveMoney(user);
	};
	auto list = std::make_unique<ChooseRecipientBoxController>(
		ChooseRecipientArgs{
			.session = session,
			.callback = std::move(callback),
			.filter = std::move(filter),
		});
	auto init = [=](not_null<PeerListBox*> box) {
		*chooser = box.get();
		box->setTitle(tr::lng_sg_nft_transfer_title());
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	};
	controller->show(
		Box<PeerListBox>(std::move(list), std::move(init)),
		Ui::LayerOption::KeepOther);
}

// Android StarGiftSheet.openTransferToAddressAlert: a paste field for a
// wallet address (ours or an external one), the box is the confirmation.
void TransferToAddressBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		NftItem item,
		Fn<void()> moved) {
	using namespace Mokagram::BoxParts;
	const auto session = &controller->session();
	const auto weak = base::make_weak(controller);
	const auto state = box->lifetime().make_state<MoveState>();

	box->setStyle(PillButtonsBox());
	RoundLayer(box);
	box->setTitle(tr::lng_sg_nft_transfer_to_address());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_sg_nft_transfer_to_address_text(
			lt_name,
			rpl::single(tr::bold(ItemName(item))),
			tr::marked),
		st::boxLabel));
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_sg_nft_address_hint()));
	box->setFocusCallback([=] { field->setFocusFast(); });

	const auto boxWeak = base::make_weak(box);
	const auto transfer = [=](const QString &address) {
		TransferNft(session, {
			.nftAddress = item.address,
			.toAddress = address,
			.requestKey = state->requestKey,
		}, [=] {
			if (const auto strong = boxWeak.get()) {
				strong->closeBox();
			}
			Moved(weak, tr::lng_sg_nft_sent(tr::now), moved);
		}, [=](const QString &type) {
			state->busy = false;
			ShowMoveError(weak, type);
		});
	};
	const auto submit = [=] {
		if (state->busy) {
			return;
		}
		const auto address = field->getLastText().trimmed();
		if (LooksLikeTonDomain(address)) {
			// name.ton -> wallet address via tondns.resolve, then move.
			state->busy = true;
			session->api().request(MTPtondns_Resolve(
				MTP_string(address.toLower())
			)).done([=](const MTPtondns_Resolved &result) {
				const auto resolved = qs(result.data().vaddress());
				if (resolved.isEmpty()) {
					state->busy = false;
					if (const auto strong = boxWeak.get()) {
						strong->showToast(
							tr::lng_sg_nft_invalid_address(tr::now));
					}
					return;
				}
				transfer(resolved);
			}).fail([=](const MTP::Error &error) {
				state->busy = false;
				ShowMoveError(weak, error.type());
			}).send();
			return;
		} else if (!LooksLikeAddress(address)) {
			field->showError();
			box->showToast(tr::lng_sg_nft_invalid_address(tr::now));
			return;
		}
		state->busy = true;
		transfer(address);
	};
	field->submits() | rpl::on_next(submit, field->lifetime());
	box->addButton(tr::lng_sg_nft_send(), submit, PillActive());
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); }, PillLight());
}

// Android StarGiftSheet.openMoveToMyGiftsAlert: payments.depositStarGift
// prepares the payload, gramwallet.transferNft moves the gift back.
void ConfirmReturnToProfile(
		not_null<Window::SessionController*> controller,
		const NftItem &item,
		Fn<void()> moved) {
	const auto session = &controller->session();
	const auto weak = base::make_weak(controller);
	const auto state = std::make_shared<MoveState>();
	const auto slug = item.gift->unique->slug;
	const auto fallback = item.address;
	controller->show(Ui::MakeConfirmBox({
		.text = tr::lng_sg_nft_return_to_profile_text(),
		.confirmed = [=](Fn<void()> close) {
			if (state->busy) {
				return;
			}
			state->busy = true;
			const auto failed = [=](const QString &type) {
				state->busy = false;
				ShowMoveError(weak, type);
			};
			PrepareGiftDeposit(session, slug, [=](
					QString nftAddress,
					QByteArray payload) {
				TransferNft(session, {
					.nftAddress = (nftAddress.isEmpty()
						? fallback
						: nftAddress),
					.depositPayload = payload,
					.requestKey = state->requestKey,
				}, [=] {
					close();
					Moved(
						weak,
						tr::lng_sg_nft_returned_to_profile(tr::now),
						moved);
				}, failed);
			}, failed);
		},
		.confirmText = tr::lng_sg_nft_move(),
		.title = tr::lng_sg_nft_return_to_profile(),
	}), Ui::LayerOption::KeepOther);
}

} // namespace

void FillNftMenu(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		const NftItem &item,
		Fn<void()> moved) {
	const auto weak = base::make_weak(controller);
	const auto unique = (item.gift && item.gift->unique)
		? item.gift->unique
		: nullptr;
	if (unique && !unique->slug.isEmpty()) {
		const auto slug = unique->slug;
		menu->addAction(tr::lng_sg_nft_open(tr::now), [=] {
			if (const auto strong = weak.get()) {
				Core::ResolveAndShowUniqueGift(strong->uiShow(), slug);
			}
		}, &st::menuIconGiftPremium);
	}
	menu->addAction(tr::lng_sg_nft_transfer_to_user(tr::now), [=] {
		if (const auto strong = weak.get()) {
			ChooseUser(strong, item, moved);
		}
	}, &st::menuIconForward);
	if (unique) {
		// Usernames and numbers can't leave StaticGram (Android SGNftSheet
		// offers them to users only).
		menu->addAction(tr::lng_sg_nft_transfer_to_address(tr::now), [=] {
			if (const auto strong = weak.get()) {
				strong->show(
					Box(TransferToAddressBox, strong, item, moved),
					Ui::LayerOption::KeepOther);
			}
		}, &st::menuIconSend);
		if (!unique->slug.isEmpty()) {
			menu->addAction(
				tr::lng_sg_nft_return_to_profile(tr::now),
				[=] {
					if (const auto strong = weak.get()) {
						ConfirmReturnToProfile(strong, item, moved);
					}
				},
				&st::menuIconProfile);
		}
	}
	const auto address = item.address;
	menu->addAction(tr::lng_sg_nft_copy_address(tr::now), [=] {
		QGuiApplication::clipboard()->setText(address);
		if (const auto strong = weak.get()) {
			strong->showToast(tr::lng_sg_nft_address_copied(tr::now));
		}
	}, &st::menuIconCopy);
}

} // namespace StaticGram::Money
