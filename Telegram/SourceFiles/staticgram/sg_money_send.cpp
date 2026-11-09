/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_money_send.h"

#include "apiwrap.h"
#include "boxes/peer_list_controllers.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/session/session_show.h"
#include "mokagram/ui/mokagram_box_parts.h"
#include "ui/boxes/emoji_stake_box.h" // AddTonInputField.
#include "ui/controls/ton_common.h"
#include "ui/controls/userpic_button.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/fields/number_input.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/vertical_list.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_chat.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_staticgram.h"

namespace StaticGram::Money {
namespace {

struct Attempt {
	int64 amount = 0;
	QString comment;
	QString target;
	QString key;
};

struct SendState {
	rpl::variable<int64> amount = 0;
	Attempt last;
	bool sending = false;
};

void AddRecipient(
		not_null<Ui::VerticalLayout*> container,
		not_null<UserData*> user) {
	const auto &st = st::sgMoneyRecipientUserpic;
	const auto wrap = container->add(
		object_ptr<Ui::RpWidget>(container),
		st::sgMoneyRecipientPadding);
	const auto userpic = Ui::CreateChild<Ui::UserpicButton>(
		wrap,
		user,
		st);
	userpic->setAttribute(Qt::WA_TransparentForMouseEvents);
	const auto name = Ui::CreateChild<Ui::FlatLabel>(
		wrap,
		rpl::single(user->name()),
		st::sgMoneyRecipientName);
	const auto status = Ui::CreateChild<Ui::FlatLabel>(
		wrap,
		tr::lng_sg_money_transfer_title(),
		st::sgMoneyRecipientStatus);
	wrap->widthValue() | rpl::on_next([=](int width) {
		const auto left = st.size.width() + st::sgMoneyRecipientSkip;
		const auto available = std::max(width - left, 1);
		name->resizeToWidth(available);
		status->resizeToWidth(available);
		const auto texts = name->height() + status->height();
		const auto height = std::max(st.size.height(), texts);
		wrap->resize(width, height);
		userpic->moveToLeft(0, (height - st.size.height()) / 2, width);
		const auto top = (height - texts) / 2;
		name->moveToLeft(left, top, width);
		status->moveToLeft(left, top + name->height(), width);
	}, wrap->lifetime());
}

// Android SGSendMoneyView «На адрес»: a wallet address or name.ton typed
// in place of the recipient plate. Desktop also takes a @username here
// (resolved to the user, then a plain gramwallet.send).
not_null<Ui::InputField*> AddAddressRecipient(
		not_null<Ui::VerticalLayout*> container) {
	Ui::AddSubsectionTitle(container, tr::lng_sg_money_address_to());
	const auto field = container->add(
		object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			tr::lng_sg_money_address_hint()),
		st::sgMoneyFieldPadding);
	auto status = field->changes(
	) | rpl::map([=] {
		const auto text = field->getLastText().trimmed();
		return text.isEmpty()
			? tr::lng_sg_money_address_explain(tr::now)
			: LooksLikeAddress(text)
			? tr::lng_sg_money_address_valid(tr::now)
			: LooksLikeTonDomain(text)
			? tr::lng_sg_money_address_domain(tr::now)
			: !ParseUsername(text).isEmpty()
			? tr::lng_sg_money_address_username(tr::now)
			: tr::lng_sg_nft_invalid_address(tr::now);
	});
	container->add(
		object_ptr<Ui::FlatLabel>(
			container,
			rpl::single(
				tr::lng_sg_money_address_explain(tr::now)
			) | rpl::then(std::move(status)),
			st::sgMoneyRecipientStatus),
		st::sgMoneyFieldPadding);
	return field;
}

void ResolveRecipient(
		not_null<Main::Session*> session,
		const QString &username,
		Fn<void(UserData*)> done) {
	session->api().request(MTPcontacts_ResolveUsername(
		MTP_flags(0),
		MTP_string(username),
		MTPstring()
	)).done([=](const MTPcontacts_ResolvedPeer &result) {
		const auto &data = result.data();
		session->data().processUsers(data.vusers());
		session->data().processChats(data.vchats());
		const auto peer = session->data().peerLoaded(
			peerFromMTP(data.vpeer()));
		done(peer ? peer->asUser() : nullptr);
	}).fail([=] {
		done(nullptr);
	}).send();
}

void SendBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		Kind kind,
		UserData *user) {
	using namespace Mokagram::BoxParts;
	const auto session = &controller->session();
	const auto state = box->lifetime().make_state<SendState>();
	const auto gram = (kind == Kind::Gram);

	box->setStyle(PillButtonsBox());
	RoundLayer(box);
	box->setWidth(st::boxWideWidth);
	box->setTitle(gram
		? tr::lng_sg_money_send_gram()
		: tr::lng_sg_money_send_stars());
	box->addTopButton(st::boxTitleClose, [=] { box->closeBox(); });

	const auto container = box->verticalLayout();
	const auto address = user
		? nullptr
		: AddAddressRecipient(container).get();
	if (user) {
		AddRecipient(container, not_null(user));
	}

	Ui::AddSubsectionTitle(
		container,
		gram
			? tr::lng_sg_money_amount_gram()
			: tr::lng_sg_money_amount_stars());

	auto text = Fn<QString()>();
	auto focus = Fn<void()>();
	auto error = Fn<void()>();
	auto fiatParent = (Ui::RpWidget*)nullptr;
	if (gram) {
		const auto field = Ui::AddTonInputField(container, {});
		field->changes() | rpl::on_next([=] {
			const auto parsed = Ui::ParseTonAmountString(
				field->getLastText());
			state->amount = std::max(parsed.value_or(0), int64(0));
		}, field->lifetime());
		text = [=] { return field->getLastText(); };
		focus = [=] { field->setFocusFast(); };
		error = [=] { field->showError(); };
		fiatParent = static_cast<Ui::RpWidget*>(field->parentWidget());
	} else {
		const auto field = Ui::AddStarsInputField(container, {
			.max = kMaxStars,
		});
		QObject::connect(field, &Ui::MaskedInputField::changed, field, [=] {
			state->amount = std::max(
				field->getLastText().toLongLong(),
				int64(0));
		});
		text = [=] { return field->getLastText(); };
		focus = [=] { field->setFocusFast(); };
		error = [=] { field->showError(); };
		fiatParent = static_cast<Ui::RpWidget*>(field->parentWidget());
	}
	box->setFocusCallback(address
		? Fn<void()>([=] { address->setFocusFast(); })
		: focus);

	// The rouble estimate on the right of the amount, like the upstream
	// AddApproximateUsd.
	const auto fiat = Ui::CreateChild<Ui::FlatLabel>(
		fiatParent,
		FiatValue(session, kind, state->amount.value()),
		st::suggestPriceEstimate);
	fiat->setAttribute(Qt::WA_TransparentForMouseEvents);
	rpl::combine(
		fiatParent->widthValue(),
		fiat->widthValue()
	) | rpl::on_next([=](int width, int) {
		fiat->moveToRight(0, st::suggestPriceEstimateTop, width);
	}, fiat->lifetime());

	container->add(
		object_ptr<Ui::FlatLabel>(
			container,
			(gram
				? tr::lng_sg_money_balance_gram
				: tr::lng_sg_money_balance_stars)(
					lt_amount,
					BalanceValue(session, kind) | rpl::map([=](int64 v) {
						return FormatAmount(kind, v);
					})),
			st::sgMoneyRecipientStatus),
		st::sgMoneyFieldPadding);

	const auto comment = container->add(
		object_ptr<Ui::InputField>(
			container,
			st::sgMoneyCommentField,
			Ui::InputField::Mode::MultiLine,
			tr::lng_sg_money_comment()),
		st::sgMoneyFieldPadding);
	comment->setMaxLength(kMaxComment);

	container->add(
		object_ptr<Ui::FlatLabel>(
			container,
			tr::lng_sg_money_help(),
			st::sgMoneyAboutLabel),
		st::sgMoneyAboutPadding);

	const auto weak = base::make_weak(controller);
	const auto send = [=] {
		if (state->sending) {
			return;
		}
		const auto amount = state->amount.current();
		if (amount <= 0 || text().isEmpty()) {
			error();
			box->showToast(tr::lng_sg_money_enter_amount(tr::now));
			return;
		} else if (!gram && amount > kMaxStars) {
			error();
			box->showToast(ErrorText(u"STARS_AMOUNT_INVALID"_q));
			return;
		}
		const auto message = comment->getLastText().trimmed();
		if (message.toUtf8().size() > kMaxComment) {
			comment->showError();
			box->showToast(ErrorText(u"WALLET_CARD_COMMENT_TOO_LONG"_q));
			return;
		}
		const auto target = address
			? address->getLastText().trimmed()
			: QString();
		const auto username = address
			&& !LooksLikeAddress(target)
			&& !LooksLikeTonDomain(target)
			? ParseUsername(target)
			: QString();
		if (address
			&& username.isEmpty()
			&& !LooksLikeAddress(target)
			&& !LooksLikeTonDomain(target)) {
			address->showError();
			box->showToast(tr::lng_sg_nft_invalid_address(tr::now));
			return;
		}
		// The same parameters reuse the key: a retry after a lost answer
		// is answered with `replayed` instead of a second transfer.
		auto &last = state->last;
		if (last.key.isEmpty()
			|| last.amount != amount
			|| last.comment != message
			|| last.target != target) {
			last = Attempt{
				.amount = amount,
				.comment = message,
				.target = target,
				.key = NewRequestKey(),
			};
		}
		state->sending = true;
		const auto boxWeak = QPointer<Ui::GenericBox>(box.get());
		const auto amountText = FormatAmount(kind, amount)
			+ u" "_q
			+ (gram ? u"GRAM"_q : u"Stars"_q);
		const auto key = last.key;
		const auto failed = [=](const QString &type) {
			const auto box = boxWeak.data();
			if (!box) {
				return;
			}
			state->sending = false;
			if (type == u"FROZEN_METHOD_INVALID"_q) {
				if (const auto strong = weak.get()) {
					if (strong->uiShow()->showFrozenError()) {
						return;
					}
				}
			}
			box->showToast(ErrorText(type));
		};
		const auto sent = [=](const QString &to, SendResult result) {
			const auto strong = weak.get();
			if (!strong) {
				return;
			}
			auto toast = tr::lng_sg_money_sent(
				tr::now,
				lt_amount,
				amountText,
				lt_user,
				to);
			if (result.fee > 0) {
				toast += u"\n"_q + tr::lng_sg_money_fee(
					tr::now,
					lt_amount,
					FormatGram(result.fee));
			}
			strong->showToast(toast);
			if (const auto box = boxWeak.data()) {
				box->closeBox();
			}
		};
		const auto sendTo = [=](not_null<UserData*> to) {
			const auto name = to->name();
			Send({
				.kind = kind,
				.to = to,
				.amount = amount,
				.comment = message,
				.requestKey = key,
			}, [=](SendResult result) {
				sent(name, result);
			}, failed);
		};
		if (user) {
			sendTo(not_null(user));
			return;
		} else if (!username.isEmpty()) {
			ResolveRecipient(session, username, [=](UserData *to) {
				if (!to || !CanReceiveMoney(to)) {
					failed(to && to->isBot()
						? u"USER_IS_BOT"_q
						: u"USER_ID_INVALID"_q);
				} else if (to->isSelf()) {
					state->sending = false;
					if (const auto box = boxWeak.data()) {
						box->showToast(tr::lng_sg_money_not_self(tr::now));
					}
				} else {
					sendTo(not_null(to));
				}
			});
			return;
		}
		SendToAddress(session, target, amount, message, key, [=](
				SendResult result) {
			sent(target, result);
		}, failed);
	};
	comment->submits() | rpl::on_next(send, comment->lifetime());

	box->addButton(
		state->amount.value() | rpl::map([=](int64 amount) {
			return (amount > 0)
				? (gram
					? tr::lng_sg_money_send_gram_amount
					: tr::lng_sg_money_send_stars_amount)(
						tr::now,
						lt_amount,
						FormatAmount(kind, amount))
				: (gram
					? tr::lng_sg_money_send_gram
					: tr::lng_sg_money_send_stars)(tr::now);
		}),
		send,
		PillActive());
}

void ChooseRecipient(
		not_null<Window::SessionController*> controller,
		Kind kind) {
	const auto session = &controller->session();
	const auto chooser = std::make_shared<QPointer<PeerListBox>>();
	const auto weak = base::make_weak(controller);
	auto callback = [=](not_null<Data::Thread*> thread) {
		const auto user = thread->peer()->asUser();
		if (const auto box = chooser->data()) {
			box->closeBox();
		}
		const auto strong = weak.get();
		if (!strong || !user) {
			return;
		} else if (user->isSelf()) {
			strong->showToast(tr::lng_sg_money_not_self(tr::now));
			return;
		}
		strong->show(
			Box(SendBox, strong, kind, user),
			Ui::LayerOption::KeepOther);
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
		box->setTitle(tr::lng_sg_money_choose_recipient());
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
		if (kind != Kind::Gram) {
			return;
		}
		// Android UserSelectorBottomSheet: «На адрес» above the users.
		auto above = object_ptr<Ui::VerticalLayout>(box);
		const auto button = above->add(object_ptr<Ui::SettingsButton>(
			above,
			tr::lng_sg_money_to_address(),
			st::settingsButtonNoIcon));
		above->add(
			object_ptr<Ui::FlatLabel>(
				above,
				tr::lng_sg_money_to_address_about(),
				st::sgMoneyRecipientStatus),
			st::sgMoneyAddressAboutPadding);
		button->setClickedCallback([=] {
			if (const auto box = chooser->data()) {
				box->closeBox();
			}
			if (const auto strong = weak.get()) {
				strong->show(
					Box(SendBox, strong, Kind::Gram, nullptr),
					Ui::LayerOption::KeepOther);
			}
		});
		box->peerListSetAboveWidget(std::move(above));
	};
	controller->show(
		Box<PeerListBox>(std::move(list), std::move(init)),
		Ui::LayerOption::KeepOther);
}

} // namespace

bool CanReceiveMoney(not_null<UserData*> user) {
	return !user->isBot()
		&& !user->isServiceUser()
		&& !user->isInaccessible()
		&& !user->isNotificationsUser()
		&& !user->isRepliesChat()
		&& !user->isVerifyCodes();
}

void ShowSendMoney(
		not_null<Window::SessionController*> controller,
		Kind kind,
		UserData *to) {
	if (!to) {
		ChooseRecipient(controller, kind);
	} else if (to->isSelf()) {
		controller->showToast(tr::lng_sg_money_not_self(tr::now));
	} else if (!CanReceiveMoney(to)) {
		controller->showToast(ErrorText(
			to->isBot() ? u"USER_IS_BOT"_q : u"USER_GIFT_UNAVAILABLE"_q));
	} else {
		controller->show(
			Box(SendBox, controller, kind, to),
			Ui::LayerOption::KeepOther);
	}
}

} // namespace StaticGram::Money
