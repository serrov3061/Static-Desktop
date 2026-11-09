/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_domains.h"

#include "staticgram/sg_box_parts.h"
#include "staticgram/sg_money_api.h"
#include "apiwrap.h"
#include "base/timer.h"
#include "base/unixtime.h"
#include "base/weak_ptr.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mokagram/ui/mokagram_box_parts.h"
#include "settings/settings_common.h"
#include "ui/layers/generic_box.h"
#include "ui/layers/show.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_sg_ui.h"
#include "styles/style_widgets.h"

#include <QtCore/QRegularExpression>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

namespace StaticGram::Domains {
namespace {

using namespace StaticGram::BoxParts;

constexpr auto kCheckDelay = crl::time(400);
constexpr auto kMintPollInterval = crl::time(3000);
constexpr auto kMintPollMax = crl::time(3 * 60 * 1000);
constexpr auto kListingsPage = 20;
constexpr auto kNanos = Money::kNanosInOne;

const auto kInfoMargins = QMargins(16, 4, 16, 8);

[[nodiscard]] QString Suffix() {
	return u".ton"_q;
}

[[nodiscard]] QString FullName(const QString &name) {
	const auto lower = name.trimmed().toLower();
	return lower.endsWith(Suffix()) ? lower : (lower + Suffix());
}

[[nodiscard]] QString ShortName(const QString &name) {
	const auto lower = name.trimmed().toLower();
	return lower.endsWith(Suffix())
		? lower.mid(0, lower.size() - Suffix().size())
		: lower;
}

[[nodiscard]] QString GramText(int64 nanos) {
	return Money::FormatGram(nanos) + u" GRAM"_q;
}

// 12.5 GRAM -> "12.5", for the price field.
[[nodiscard]] QString PlainGrams(int64 nanos) {
	auto result = QString::number(nanos / kNanos);
	auto fraction = QString::number(nanos % kNanos).rightJustified(
		9,
		QChar('0'));
	while (fraction.endsWith(QChar('0'))) {
		fraction.chop(1);
	}
	return fraction.isEmpty() ? result : (result + '.' + fraction);
}

// "12.5" / "12,5" -> nanoton, 0 when it is not an amount.
[[nodiscard]] int64 ParseGrams(QString text) {
	text = text.trimmed().replace(QChar(','), QChar('.'));
	static const auto re = QRegularExpression(
		u"^(\\d{1,9})(?:\\.(\\d{0,9}))?$"_q);
	const auto match = re.match(text);
	if (!match.hasMatch()) {
		return 0;
	}
	const auto whole = match.captured(1).toLongLong();
	const auto fraction = match.captured(2).leftJustified(
		9,
		QChar('0')).toLongLong();
	return whole * kNanos + fraction;
}

[[nodiscard]] QString ShortAddress(const QString &address) {
	return (address.size() > 14)
		? (address.left(6) + QString::fromUtf8("\xE2\x80\xA6") + address.right(6))
		: address;
}

[[nodiscard]] QString DateText(TimeId date) {
	return langDayOfMonthFull(base::unixtime::parse(date).date());
}

[[nodiscard]] bool IsUnknownMethod(const MTP::Error &error) {
	const auto &type = error.type();
	return type.contains(u"METHOD_INVALID"_q)
		|| type.contains(u"INPUT_CONSTRUCTOR"_q)
		|| type.contains(u"CONSTRUCTOR_INVALID"_q)
		|| type.contains(u"CONNECTION_LAYER_INVALID"_q)
		|| type.contains(u"INPUT_METHOD"_q)
		|| (error.code() == 501);
}

// The edge or the server does not know a newer request yet: a human text,
// never the raw code (Android SGTonDns.errorText).
[[nodiscard]] QString ErrorType(const MTP::Error &error) {
	return IsUnknownMethod(error) ? u"TONDNS_UNAVAILABLE"_q : error.type();
}

[[nodiscard]] QString ErrorText(const QString &type) {
	static const auto map = base::flat_map<QString, tr::phrase<>>{
		{ u"TONDNS_NAME_INVALID"_q, tr::lng_sg_dns_error_invalid },
		{ u"TONDNS_NAME_TAKEN"_q, tr::lng_sg_dns_error_taken },
		{ u"TONDNS_NOT_FOUND"_q, tr::lng_sg_dns_error_not_found },
		{ u"TONDNS_NOT_OWNER"_q, tr::lng_sg_dns_error_not_owner },
		{ u"TONDNS_PRICE_CHANGED"_q, tr::lng_sg_dns_error_price_changed },
		{ u"TONDNS_ADDRESS_INVALID"_q, tr::lng_sg_dns_error_address },
		{ u"TONDNS_UNAVAILABLE"_q, tr::lng_sg_dns_error_unavailable },
		{
			u"TONDNS_REQUEST_KEY_CONFLICT"_q,
			tr::lng_sg_dns_error_key_conflict,
		},
		{ u"BALANCE_TOO_LOW"_q, tr::lng_sg_dns_error_balance },
		{ u"TONDNS_NOT_LISTED"_q, tr::lng_sg_dns_error_not_listed },
		{ u"TONDNS_SELF_BUY"_q, tr::lng_sg_dns_error_self_buy },
		{ u"TONDNS_PRICE_INVALID"_q, tr::lng_sg_dns_error_price_invalid },
		{ u"TONDNS_NOT_MINTED"_q, tr::lng_sg_dns_error_not_minted },
		{
			u"TONDNS_SALE_UNAVAILABLE"_q,
			tr::lng_sg_dns_error_sale_unavailable,
		},
		{ u"TONDNS_BUSY"_q, tr::lng_sg_dns_error_busy },
	};
	const auto i = map.find(type);
	return (i != end(map))
		? i->second(tr::now)
		: tr::lng_sg_dns_error_unknown(tr::now);
}

struct Domain {
	QString name; // "alice.ton"
	QString itemAddress;
	QString ownerAddress;
	QString walletAddress;
	int64 price = 0;
	TimeId purchasedAt = 0;
	bool primary = false;
	bool minted = false;
};

struct Listing {
	QString name; // "alice.ton"
	UserId sellerId;
	int64 price = 0;
	QString itemAddress;
	TimeId listedAt = 0;
};

struct Check {
	QString name; // "alice.ton"
	int64 price = 0;
	bool available = false;
	std::optional<Listing> listing;
};

[[nodiscard]] Domain ParseDomain(const MTPtondns_Domain &domain) {
	const auto &data = domain.data();
	return {
		.name = FullName(qs(data.vname())),
		.itemAddress = qs(data.vitem_address()),
		.ownerAddress = qs(data.vowner_address()),
		.walletAddress = qs(data.vwallet_address()),
		.price = int64(data.vprice().v),
		.purchasedAt = data.vpurchased_at().v,
		.primary = data.is_primary(),
		.minted = data.is_minted(),
	};
}

[[nodiscard]] Listing ParseListing(const MTPtondns_Listing &listing) {
	const auto &data = listing.data();
	return {
		.name = FullName(qs(data.vname())),
		.sellerId = UserId(data.vseller_id()),
		.price = int64(data.vprice().v),
		.itemAddress = qs(data.vitem_address()),
		.listedAt = data.vlisted_at().v,
	};
}

// Per account (Android SGTonDns): the own domains and listings, the resale
// bounds, and the polling of the mints that are not finished yet.
class Controller final : public base::has_weak_ptr {
public:
	explicit Controller(not_null<Main::Session*> session);

	[[nodiscard]] not_null<Main::Session*> session() const {
		return _session;
	}
	[[nodiscard]] rpl::producer<> updates() const {
		return _updates.events();
	}

	[[nodiscard]] const std::vector<Domain> &my() const {
		return _my;
	}
	[[nodiscard]] bool myLoaded() const {
		return _myLoaded;
	}
	[[nodiscard]] bool myFailed() const {
		return _myFailed;
	}
	[[nodiscard]] const Domain *findMine(const QString &name) const;
	[[nodiscard]] const Listing *myListing(const QString &name) const;
	[[nodiscard]] int64 saleMin() const {
		return _saleMin;
	}
	[[nodiscard]] int64 saleMax() const {
		return _saleMax;
	}
	[[nodiscard]] int saleFee() const {
		return _saleFee;
	}

	void setShow(std::shared_ptr<Ui::Show> show);
	void loadMy(bool poll = false);
	void loadMyListings();
	mtpRequestId check(
		const QString &name,
		Fn<void(Check)> done,
		Fn<void(QString)> fail);
	void cancel(mtpRequestId requestId);
	void buy(
		const QString &name,
		int64 price,
		const QString &requestKey,
		Fn<void(Domain)> done,
		Fn<void(QString)> fail);
	void buyListed(
		const Listing &listing,
		const QString &requestKey,
		Fn<void()> done,
		Fn<void(QString)> fail);
	void setPrimary(
		const QString &name,
		Fn<void()> done,
		Fn<void(QString)> fail);
	void setWalletRecord(
		const QString &name,
		const QString &address,
		Fn<void()> done,
		Fn<void(QString)> fail);
	void listForSale(
		const QString &name,
		int64 price,
		Fn<void()> done,
		Fn<void(QString)> fail);
	void cancelSale(
		const QString &name,
		Fn<void()> done,
		Fn<void(QString)> fail);
	void loadListings(
		int offset,
		Fn<void(std::vector<Listing>, int)> done,
		Fn<void()> fail);

private:
	mtpRequestId checkPlain(
		const QString &name,
		Fn<void(Check)> done,
		Fn<void(QString)> fail);
	void replaceMine(Domain domain);
	void applyListingsMeta(const MTPDtondns_listings &data);
	void checkMintedTransitions();
	[[nodiscard]] bool hasPendingMint() const;
	[[nodiscard]] bool mintPollExpired() const;
	void pollMints();

	const not_null<Main::Session*> _session;
	std::vector<Domain> _my;
	base::flat_map<QString, Listing> _myListings;
	base::flat_set<QString> _expected; // Bought listings not in _my yet.
	base::flat_set<QString> _minting; // Last seen not minted.
	std::shared_ptr<Ui::Show> _show;
	base::Timer _mintTimer;
	crl::time _mintPollStarted = 0;
	mtpRequestId _myRequestId = 0;
	int64 _saleMin = kNanos;
	int64 _saleMax = int64(1'000'000) * kNanos;
	int _saleFee = 10;
	bool _myLoaded = false;
	bool _myFailed = false;
	bool _checkEx = true;
	rpl::event_stream<> _updates;

};

Controller::Controller(not_null<Main::Session*> session)
: _session(session)
, _mintTimer([=] { loadMy(true); }) {
}

const Domain *Controller::findMine(const QString &name) const {
	const auto full = FullName(name);
	const auto i = ranges::find(_my, full, &Domain::name);
	return (i != end(_my)) ? &*i : nullptr;
}

const Listing *Controller::myListing(const QString &name) const {
	const auto i = _myListings.find(FullName(name));
	return (i != end(_myListings)) ? &i->second : nullptr;
}

void Controller::setShow(std::shared_ptr<Ui::Show> show) {
	_show = std::move(show);
}

void Controller::loadMy(bool poll) {
	if (_myRequestId) {
		return;
	}
	_myRequestId = _session->api().request(MTPtondns_GetMyDomains(
	)).done(crl::guard(this, [=](const MTPtondns_Domains &result) {
		_myRequestId = 0;
		auto list = std::vector<Domain>();
		for (const auto &domain : result.data().vdomains().v) {
			list.push_back(ParseDomain(domain));
		}
		_my = std::move(list);
		_myLoaded = true;
		_myFailed = false;
		checkMintedTransitions();
		if (poll && mintPollExpired()) {
			_mintPollStarted = 0;
			_expected.clear();
		} else {
			pollMints();
		}
		_updates.fire({});
	})).fail(crl::guard(this, [=] {
		_myRequestId = 0;
		_myFailed = !_myLoaded;
		if (poll && !mintPollExpired()) {
			pollMints();
		}
		_updates.fire({});
	})).send();
}

void Controller::checkMintedTransitions() {
	auto ready = std::vector<QString>();
	for (const auto &domain : _my) {
		if (domain.minted) {
			const auto wasMinting = _minting.remove(domain.name);
			const auto wasExpected = _expected.remove(domain.name);
			if (wasMinting || wasExpected) {
				ready.push_back(domain.name);
			}
		} else {
			_minting.emplace(domain.name);
			_expected.remove(domain.name);
		}
	}
	if (!_show || !_show->valid()) {
		return;
	}
	for (const auto &name : ready) {
		_show->showToast(tr::lng_sg_dns_domain_ready(
			tr::now,
			lt_name,
			name));
	}
}

bool Controller::hasPendingMint() const {
	return !_expected.empty()
		|| ranges::any_of(_my, [](const Domain &d) { return !d.minted; });
}

bool Controller::mintPollExpired() const {
	return _mintPollStarted
		&& (crl::now() - _mintPollStarted > kMintPollMax);
}

void Controller::pollMints() {
	if (!hasPendingMint()) {
		_mintTimer.cancel();
		_mintPollStarted = 0;
		return;
	}
	if (!_mintPollStarted) {
		_mintPollStarted = crl::now();
	}
	_mintTimer.callOnce(kMintPollInterval);
}

void Controller::replaceMine(Domain domain) {
	const auto i = ranges::find(_my, domain.name, &Domain::name);
	if (i != end(_my)) {
		*i = std::move(domain);
	} else {
		_my.insert(begin(_my), std::move(domain));
	}
	_updates.fire({});
}

void Controller::applyListingsMeta(const MTPDtondns_listings &data) {
	if (data.vmax_price().v > 0) {
		_saleMin = int64(data.vmin_price().v);
		_saleMax = int64(data.vmax_price().v);
	}
	if (data.vfee_percent().v >= 0) {
		_saleFee = data.vfee_percent().v;
	}
	_session->data().processUsers(data.vusers());
}

void Controller::loadMyListings() {
	_session->api().request(MTPtondns_GetMyListings(
	)).done(crl::guard(this, [=](const MTPtondns_Listings &result) {
		const auto &data = result.data();
		applyListingsMeta(data);
		_myListings.clear();
		for (const auto &listing : data.vlistings().v) {
			auto parsed = ParseListing(listing);
			auto name = parsed.name;
			_myListings.emplace(std::move(name), std::move(parsed));
		}
		_updates.fire({});
	})).send();
}

mtpRequestId Controller::check(
		const QString &name,
		Fn<void(Check)> done,
		Fn<void(QString)> fail) {
	if (!_checkEx) {
		return checkPlain(name, std::move(done), std::move(fail));
	}
	return _session->api().request(MTPtondns_CheckDomainEx(
		MTP_string(name)
	)).done(crl::guard(this, [=](const MTPtondns_DomainCheckEx &result) {
		const auto &data = result.data();
		_session->data().processUsers(data.vusers());
		auto check = Check{
			.name = FullName(qs(data.vname())),
			.price = int64(data.vprice().v),
			.available = data.is_available(),
		};
		if (const auto listing = data.vlisting()) {
			check.listing = ParseListing(*listing);
		}
		done(std::move(check));
	})).fail(crl::guard(this, [=](const MTP::Error &error) {
		if (IsUnknownMethod(error)) {
			// An older server: plain checkDomain without the listing.
			_checkEx = false;
			checkPlain(name, done, fail);
			return;
		}
		fail(error.type());
	})).send();
}

mtpRequestId Controller::checkPlain(
		const QString &name,
		Fn<void(Check)> done,
		Fn<void(QString)> fail) {
	return _session->api().request(MTPtondns_CheckDomain(
		MTP_string(name)
	)).done([=](const MTPtondns_DomainCheck &result) {
		const auto &data = result.data();
		done(Check{
			.name = FullName(qs(data.vname())),
			.price = int64(data.vprice().v),
			.available = mtpIsTrue(data.vavailable()),
		});
	}).fail([=](const MTP::Error &error) {
		fail(ErrorType(error));
	}).send();
}

void Controller::cancel(mtpRequestId requestId) {
	if (requestId) {
		_session->api().request(requestId).cancel();
	}
}

void Controller::buy(
		const QString &name,
		int64 price,
		const QString &requestKey,
		Fn<void(Domain)> done,
		Fn<void(QString)> fail) {
	_session->api().request(MTPtondns_BuyDomain(
		MTP_string(ShortName(name)),
		MTP_long(price),
		MTP_string(requestKey)
	)).done(crl::guard(this, [=](const MTPtondns_BuyResult &result) {
		auto domain = ParseDomain(result.data().vdomain());
		if (!domain.minted) {
			_minting.emplace(domain.name);
		}
		replaceMine(domain);
		Money::RefreshBalances(_session);
		_mintPollStarted = 0;
		pollMints();
		done(std::move(domain));
	})).fail([=](const MTP::Error &error) {
		fail(ErrorType(error));
	}).send();
}

void Controller::buyListed(
		const Listing &listing,
		const QString &requestKey,
		Fn<void()> done,
		Fn<void(QString)> fail) {
	const auto name = listing.name;
	_session->api().request(MTPtondns_BuyListed(
		MTP_string(name),
		MTP_long(listing.price),
		MTP_string(requestKey)
	)).done(crl::guard(this, [=] {
		Money::RefreshBalances(_session);
		// The item moves to our wallet within seconds, getMyDomains lists
		// it once it lands.
		_expected.emplace(name);
		_mintPollStarted = 0;
		pollMints();
		done();
	})).fail([=](const MTP::Error &error) {
		fail(ErrorType(error));
	}).send();
}

void Controller::setPrimary(
		const QString &name,
		Fn<void()> done,
		Fn<void(QString)> fail) {
	const auto full = name.isEmpty() ? QString() : FullName(name);
	_session->api().request(MTPtondns_SetPrimaryDomain(
		MTP_string(full)
	)).done(crl::guard(this, [=](const MTPBool &result) {
		if (!mtpIsTrue(result)) {
			fail(QString());
			return;
		}
		for (auto &domain : _my) {
			domain.primary = !full.isEmpty() && (domain.name == full);
		}
		_updates.fire({});
		done();
	})).fail([=](const MTP::Error &error) {
		fail(ErrorType(error));
	}).send();
}

void Controller::setWalletRecord(
		const QString &name,
		const QString &address,
		Fn<void()> done,
		Fn<void(QString)> fail) {
	using Flag = MTPtondns_setWalletRecord::Flag;
	const auto trimmed = address.trimmed();
	_session->api().request(MTPtondns_SetWalletRecord(
		MTP_flags(trimmed.isEmpty() ? Flag() : Flag::f_address),
		MTP_string(FullName(name)),
		MTP_string(trimmed)
	)).done(crl::guard(this, [=](const MTPtondns_Domain &result) {
		auto domain = ParseDomain(result);
		if (const auto mine = findMine(domain.name); mine && mine->primary) {
			// Keep the local primary flag if the answer omits it.
			domain.primary = true;
		}
		replaceMine(std::move(domain));
		done();
	})).fail([=](const MTP::Error &error) {
		fail(ErrorType(error));
	}).send();
}

void Controller::listForSale(
		const QString &name,
		int64 price,
		Fn<void()> done,
		Fn<void(QString)> fail) {
	_session->api().request(MTPtondns_ListForSale(
		MTP_string(FullName(name)),
		MTP_long(price)
	)).done(crl::guard(this, [=](const MTPtondns_Listing &result) {
		auto listing = ParseListing(result);
		auto key = listing.name;
		_myListings.remove(key);
		_myListings.emplace(std::move(key), std::move(listing));
		_updates.fire({});
		done();
	})).fail([=](const MTP::Error &error) {
		fail(ErrorType(error));
	}).send();
}

void Controller::cancelSale(
		const QString &name,
		Fn<void()> done,
		Fn<void(QString)> fail) {
	const auto full = FullName(name);
	const auto removed = crl::guard(this, [=] {
		_myListings.remove(full);
		_updates.fire({});
		done();
	});
	_session->api().request(MTPtondns_CancelSale(
		MTP_string(full)
	)).done([=](const MTPBool &result) {
		if (mtpIsTrue(result)) {
			removed();
		} else {
			fail(QString());
		}
	}).fail([=](const MTP::Error &error) {
		if (error.type() == u"TONDNS_NOT_LISTED"_q) {
			removed();
		} else {
			fail(ErrorType(error));
		}
	}).send();
}

void Controller::loadListings(
		int offset,
		Fn<void(std::vector<Listing>, int)> done,
		Fn<void()> fail) {
	_session->api().request(MTPtondns_GetListings(
		MTP_int(offset),
		MTP_int(kListingsPage),
		MTP_string(u"date"_q)
	)).done(crl::guard(this, [=](const MTPtondns_Listings &result) {
		const auto &data = result.data();
		applyListingsMeta(data);
		auto list = std::vector<Listing>();
		for (const auto &listing : data.vlistings().v) {
			list.push_back(ParseListing(listing));
		}
		done(std::move(list), data.vcount().v);
	})).fail([=] {
		fail();
	}).send();
}

[[nodiscard]] not_null<Controller*> Instance(
		not_null<Main::Session*> session) {
	static auto map = base::flat_map<
		not_null<Main::Session*>,
		std::unique_ptr<Controller>>();
	auto i = map.find(session);
	if (i == end(map)) {
		i = map.emplace(
			session,
			std::make_unique<Controller>(session)).first;
		session->lifetime().add([=] {
			map.remove(session);
		});
	}
	return i->second.get();
}

[[nodiscard]] QString SellerName(
		not_null<Main::Session*> session,
		UserId id) {
	if (const auto user = session->data().userLoaded(id)) {
		return user->name();
	}
	return QString::number(id.bare);
}

// A label / value row inside a card, not clickable unless a callback.
not_null<Ui::SettingsButton*> AddInfoRow(
		not_null<Ui::VerticalLayout*> card,
		rpl::producer<QString> label,
		rpl::producer<QString> value,
		Fn<void()> callback = nullptr) {
	const auto button = Settings::AddButtonWithLabel(
		card,
		std::move(label),
		std::move(value),
		st::settingsButtonNoIcon);
	if (callback) {
		button->setClickedCallback(std::move(callback));
	} else {
		button->setAttribute(Qt::WA_TransparentForMouseEvents);
	}
	return button;
}

void AddText(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		const style::FlatLabel &st,
		QMargins margins = QMargins(16, 4, 16, 4)) {
	container->add(
		object_ptr<Ui::FlatLabel>(container, std::move(text), st),
		margins);
}

void ShowDomainBox(
	not_null<Window::SessionController*> window,
	not_null<Controller*> dns,
	const QString &name);

// «Купить name.ton»: price, balance, «Навсегда», then minting / done with
// «Сделать основным» (Android SGDomainsActivity.openBuySheet).
void BuyBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> window,
		not_null<Controller*> dns,
		QString name,
		int64 price,
		Fn<void()> changed) {
	const auto session = dns->session();
	SetupBox(box);
	box->setTitle(rpl::single(name));
	box->setWidth(st::boxWideWidth);

	enum class Stage {
		Confirm,
		Minting,
		Done,
	};
	struct State {
		rpl::variable<int64> price;
		rpl::variable<int64> balance = -1;
		QString requestKey = Money::NewRequestKey();
		Stage stage = Stage::Confirm;
		Stage shown = Stage::Confirm;
		bool loading = false;
		bool primaryShown = false;
		Fn<void()> update;
	};
	const auto state = box->lifetime().make_state<State>();
	state->price = price;

	Money::GramBalanceValue(
		session
	) | rpl::on_next([=](int64 balance) {
		state->balance = balance;
	}, box->lifetime());

	const auto container = box->verticalLayout();
	const auto body = container->add(
		object_ptr<Ui::VerticalLayout>(container));

	const auto fill = [=] {
		body->clear();
		state->shown = state->stage;
		if (state->stage == Stage::Confirm) {
			const auto card = AddCard(body, 0, 6);
			AddInfoRow(
				card,
				tr::lng_sg_dns_price(),
				state->price.value() | rpl::map(GramText));
			AddInfoRow(
				card,
				tr::lng_sg_dns_balance(),
				state->balance.value() | rpl::map([](int64 balance) {
					return (balance < 0)
						? tr::lng_sg_dns_loading(tr::now)
						: GramText(balance);
				}));
			AddInfoRow(
				card,
				tr::lng_sg_dns_term(),
				tr::lng_sg_dns_term_forever());
			AddCardNote(body, tr::lng_sg_dns_buy_text());
		} else {
			const auto minting = (state->stage == Stage::Minting);
			AddText(
				body,
				(minting
					? tr::lng_sg_dns_minting()
					: tr::lng_sg_dns_bought()),
				st::sgDnsStage,
				QMargins(16, 12, 16, 8));
			AddText(
				body,
				(minting
					? tr::lng_sg_dns_minting_text()
					: tr::lng_sg_dns_bought_text(
						lt_name,
						rpl::single(name))),
				st::sgCardNote,
				QMargins(24, 0, 24, 12));
		}
		body->resizeToWidth(container->width());
	};

	const auto buy = [=] {
		if (state->loading) {
			return;
		}
		state->loading = true;
		dns->buy(name, state->price.current(), state->requestKey, crl::guard(box, [=](
				Domain domain) {
			state->loading = false;
			state->stage = domain.minted ? Stage::Done : Stage::Minting;
			state->update();
			if (changed) {
				changed();
			}
		}), crl::guard(box, [=](QString type) {
			state->loading = false;
			box->showToast(ErrorText(type));
			if (type != u"TONDNS_PRICE_CHANGED"_q) {
				return;
			}
			// Show the live price before buying again.
			dns->check(ShortName(name), crl::guard(box, [=](Check check) {
				if (!check.available) {
					box->closeBox();
					if (changed) {
						changed();
					}
					return;
				}
				state->price = check.price;
			}), [](QString) {});
		}));
	};

	const auto refreshButtons = [=] {
		box->clearButtons();
		if (state->stage == Stage::Confirm) {
			const auto balance = state->balance.current();
			const auto low = (balance >= 0)
				&& (balance < state->price.current());
			if (low) {
				box->addButton(tr::lng_sg_dns_low_balance(), [] {});
			} else {
				box->addButton(
					tr::lng_sg_dns_buy_for(
						lt_amount,
						state->price.value() | rpl::map(GramText)),
					buy,
					Mokagram::BoxParts::PillActive());
			}
			box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
		} else if (state->stage == Stage::Minting) {
			box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		} else {
			const auto mine = dns->findMine(name);
			if (!mine || !mine->primary) {
				box->addButton(tr::lng_sg_dns_set_primary(), [=] {
					if (state->loading) {
						return;
					}
					state->loading = true;
					dns->setPrimary(name, crl::guard(box, [=] {
						state->loading = false;
						window->showToast(tr::lng_sg_dns_primary_set(
							tr::now,
							lt_name,
							name));
						box->closeBox();
					}), crl::guard(box, [=](QString type) {
						state->loading = false;
						box->showToast(ErrorText(type));
					}));
				}, Mokagram::BoxParts::PillActive());
			}
			box->addButton(tr::lng_sg_dns_done(), [=] { box->closeBox(); });
		}
	};

	state->update = [=] {
		const auto mine = dns->findMine(name);
		if (state->stage == Stage::Minting && mine && mine->minted) {
			state->stage = Stage::Done;
		}
		const auto primary = mine && mine->primary;
		if (state->shown != state->stage
			|| state->primaryShown != primary) {
			state->primaryShown = primary;
			fill();
		}
		refreshButtons();
	};
	fill();
	refreshButtons();

	rpl::merge(
		dns->updates(),
		state->balance.changes() | rpl::to_empty,
		state->price.changes() | rpl::to_empty
	) | rpl::on_next([=] {
		state->update();
	}, box->lifetime());
}

// A listed domain of another user (Android openListedBuySheet).
void BuyListedBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> window,
		not_null<Controller*> dns,
		Listing listing,
		Fn<void()> changed) {
	const auto session = dns->session();
	SetupBox(box);
	box->setTitle(rpl::single(listing.name));
	box->setWidth(st::boxWideWidth);

	struct State {
		Listing listing;
		rpl::variable<int64> price;
		rpl::variable<int64> balance = -1;
		QString requestKey = Money::NewRequestKey();
		bool loading = false;
	};
	const auto state = box->lifetime().make_state<State>();
	state->listing = listing;
	state->price = listing.price;

	Money::GramBalanceValue(
		session
	) | rpl::on_next([=](int64 balance) {
		state->balance = balance;
	}, box->lifetime());

	const auto container = box->verticalLayout();
	const auto card = AddCard(container, 0, 6);
	AddInfoRow(
		card,
		tr::lng_sg_dns_price(),
		state->price.value() | rpl::map(GramText));
	AddInfoRow(
		card,
		tr::lng_sg_dns_balance(),
		state->balance.value() | rpl::map([](int64 balance) {
			return (balance < 0)
				? tr::lng_sg_dns_loading(tr::now)
				: GramText(balance);
		}));
	const auto sellerId = listing.sellerId;
	AddInfoRow(
		card,
		tr::lng_sg_dns_seller(),
		rpl::single(SellerName(session, sellerId)),
		[=] {
			if (const auto user = session->data().userLoaded(sellerId)) {
				window->showPeerInfo(user);
			}
		});
	AddCardNote(container, tr::lng_sg_dns_buy_listed_text());

	const auto finished = [=] {
		box->closeBox();
		if (changed) {
			changed();
		}
	};
	const auto buy = [=] {
		if (state->loading) {
			return;
		}
		state->loading = true;
		auto current = state->listing;
		current.price = state->price.current();
		dns->buyListed(current, state->requestKey, crl::guard(box, [=] {
			window->showToast(tr::lng_sg_dns_bought_listed(tr::now));
			finished();
		}), crl::guard(box, [=](QString type) {
			state->loading = false;
			if (type == u"TONDNS_PRICE_CHANGED"_q) {
				// A new price is a new purchase: a new request key.
				dns->check(
					ShortName(state->listing.name),
					crl::guard(box, [=](Check check) {
						if (!check.listing) {
							window->showToast(
								tr::lng_sg_dns_error_not_listed(tr::now));
							finished();
							return;
						}
						state->requestKey = Money::NewRequestKey();
						state->listing = *check.listing;
						state->price = check.listing->price;
						box->showToast(tr::lng_sg_dns_price_changed_new(
							tr::now,
							lt_amount,
							Money::FormatGram(check.listing->price)));
					}),
					[](QString) {});
			} else if (type == u"TONDNS_NOT_LISTED"_q) {
				window->showToast(ErrorText(type));
				finished();
			} else {
				box->showToast(ErrorText(type));
			}
		}));
	};

	const auto refreshButtons = [=] {
		box->clearButtons();
		const auto balance = state->balance.current();
		if (balance >= 0 && balance < state->price.current()) {
			box->addButton(tr::lng_sg_dns_low_balance(), [] {});
		} else {
			box->addButton(
				tr::lng_sg_dns_buy_for(
					lt_amount,
					state->price.value() | rpl::map(GramText)),
				buy,
				Mokagram::BoxParts::PillActive());
		}
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	};
	refreshButtons();
	rpl::merge(
		state->balance.changes() | rpl::to_empty,
		state->price.changes() | rpl::to_empty
	) | rpl::on_next(refreshButtons, box->lifetime());
}

// «Адрес кошелька» of a domain (Android SGDomainSheet.editWalletRecord).
void WalletRecordBox(
		not_null<Ui::GenericBox*> box,
		not_null<Controller*> dns,
		QString name) {
	SetupBox(box);
	box->setTitle(tr::lng_sg_dns_wallet_record());
	box->setWidth(st::boxWideWidth);

	const auto mine = dns->findMine(name);
	const auto current = (mine
		&& !mine->walletAddress.isEmpty()
		&& mine->walletAddress != mine->ownerAddress)
		? mine->walletAddress
		: QString();

	const auto container = box->verticalLayout();
	AddCardNote(
		container,
		tr::lng_sg_dns_wallet_record_text(lt_name, rpl::single(name)));
	const auto card = AddCard(container, 0, 6);
	const auto field = card->add(
		object_ptr<Ui::InputField>(
			card,
			Mokagram::BoxParts::CardField(st::defaultInputField),
			tr::lng_sg_dns_wallet_hint(),
			current),
		QMargins());
	box->setFocusCallback([=] {
		field->setFocusFast();
	});

	const auto loading = box->lifetime().make_state<bool>(false);
	const auto save = [=](const QString &address) {
		if (*loading) {
			return;
		}
		*loading = true;
		dns->setWalletRecord(name, address, crl::guard(box, [=] {
			box->showToast(tr::lng_sg_dns_wallet_saved(tr::now));
			box->closeBox();
		}), crl::guard(box, [=](QString type) {
			*loading = false;
			box->showToast(ErrorText(type));
		}));
	};
	const auto submit = [=] {
		const auto address = field->getLastText().trimmed();
		if (address.isEmpty()) {
			field->showError();
			return;
		}
		save(address);
	};
	field->submits() | rpl::on_next(submit, field->lifetime());
	box->addButton(
		tr::lng_settings_save(),
		submit,
		Mokagram::BoxParts::PillActive());
	box->addButton(tr::lng_sg_dns_wallet_reset(), [=] {
		save(QString());
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

// «Продать name.ton» (Android SGDomainSaleSheet): a fixed GRAM price, what
// the seller gets after the fee, change the price or take it off sale.
void SaleBox(
		not_null<Ui::GenericBox*> box,
		not_null<Controller*> dns,
		QString name) {
	SetupBox(box);
	box->setTitle(tr::lng_sg_dns_sale_title(lt_name, rpl::single(name)));
	box->setWidth(st::boxWideWidth);

	const auto initial = dns->myListing(name);
	const auto container = box->verticalLayout();
	const auto card = AddCard(container, 0, 6);
	const auto field = card->add(
		object_ptr<Ui::InputField>(
			card,
			Mokagram::BoxParts::CardField(st::defaultInputField),
			tr::lng_sg_dns_sale_price_hint(),
			initial ? PlainGrams(initial->price) : QString()),
		QMargins());
	box->setFocusCallback([=] {
		field->setFocusFast();
	});
	const auto info = container->add(
		object_ptr<Ui::FlatLabel>(container, QString(), st::sgCardNote),
		QMargins(
			Mokagram::BoxParts::kOuterSide + 16,
			2,
			Mokagram::BoxParts::kOuterSide + 16,
			2));
	const auto receive = container->add(
		object_ptr<Ui::FlatLabel>(container, QString(), st::sgCardNote),
		QMargins(
			Mokagram::BoxParts::kOuterSide + 16,
			2,
			Mokagram::BoxParts::kOuterSide + 16,
			8));

	const auto loading = box->lifetime().make_state<bool>(false);
	const auto bind = [=] {
		const auto listing = dns->myListing(name);
		info->setText(listing
			? tr::lng_sg_dns_sale_listed_for(
				tr::now,
				lt_amount,
				Money::FormatGram(listing->price))
			: tr::lng_sg_dns_sale_bounds(
				tr::now,
				lt_min,
				Money::FormatGram(dns->saleMin()),
				lt_max,
				Money::FormatGram(dns->saleMax())));
		const auto price = ParseGrams(field->getLastText());
		const auto fee = dns->saleFee();
		receive->setText((price > 0)
			? tr::lng_sg_dns_sale_you_get(
				tr::now,
				lt_amount,
				Money::FormatGram(price - price * fee / 100),
				lt_percent,
				QString::number(fee))
			: tr::lng_sg_dns_sale_fee(
				tr::now,
				lt_percent,
				QString::number(fee)));
	};
	const auto list = [=] {
		if (*loading) {
			return;
		}
		const auto price = ParseGrams(field->getLastText());
		if (price < dns->saleMin() || price > dns->saleMax()) {
			field->showError();
			box->showToast(tr::lng_sg_dns_error_price_invalid(tr::now));
			return;
		}
		*loading = true;
		dns->listForSale(name, price, crl::guard(box, [=] {
			box->showToast(tr::lng_sg_dns_sale_listed(
				tr::now,
				lt_name,
				name));
			box->closeBox();
		}), crl::guard(box, [=](QString type) {
			*loading = false;
			box->showToast(ErrorText(type));
		}));
	};
	const auto refreshButtons = [=] {
		box->clearButtons();
		const auto listing = dns->myListing(name);
		box->addButton(
			(listing
				? tr::lng_sg_dns_sale_change()
				: tr::lng_sg_dns_sale_list()),
			list,
			Mokagram::BoxParts::PillActive());
		if (listing) {
			box->addButton(tr::lng_sg_dns_sale_cancel(), [=] {
				if (*loading) {
					return;
				}
				*loading = true;
				dns->cancelSale(name, crl::guard(box, [=] {
					box->showToast(tr::lng_sg_dns_sale_cancelled(tr::now));
					box->closeBox();
				}), crl::guard(box, [=](QString type) {
					*loading = false;
					box->showToast(ErrorText(type));
				}));
			}, Mokagram::BoxParts::PillAttention());
		}
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	};
	bind();
	refreshButtons();
	field->changes() | rpl::on_next(bind, field->lifetime());
	field->submits() | rpl::on_next(list, field->lifetime());
	dns->updates() | rpl::on_next([=] {
		bind();
		refreshButtons();
	}, box->lifetime());
	dns->loadMyListings(); // The bounds, the fee, the current listing.
}

[[nodiscard]] QString StatusText(
		not_null<Controller*> dns,
		const Domain &domain) {
	auto status = QString();
	if (const auto listing = dns->myListing(domain.name)) {
		status = tr::lng_sg_dns_sale_listed_for(
			tr::now,
			lt_amount,
			Money::FormatGram(listing->price));
	} else if (!domain.minted) {
		status = tr::lng_sg_dns_status_minting(tr::now);
	} else {
		status = tr::lng_sg_dns_status_ready(tr::now);
	}
	return domain.primary
		? (tr::lng_sg_dns_primary_chip(tr::now)
			+ QString::fromUtf8(" \xC2\xB7 ")
			+ status)
		: status;
}

// One own domain (Android SGDomainSheet): status, wallet record, the
// listing or the purchase date, the item address; primary and sale.
void DomainBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> window,
		not_null<Controller*> dns,
		QString name) {
	SetupBox(box);
	box->setTitle(rpl::single(name));
	box->setWidth(st::boxWideWidth);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });

	const auto loading = box->lifetime().make_state<bool>(false);
	const auto container = box->verticalLayout();
	const auto body = container->add(
		object_ptr<Ui::VerticalLayout>(container));

	const auto togglePrimary = [=] {
		const auto mine = dns->findMine(name);
		if (!mine) {
			box->showToast(tr::lng_sg_dns_error_not_owner(tr::now));
			return;
		} else if (*loading) {
			return;
		}
		const auto primary = mine->primary;
		*loading = true;
		dns->setPrimary(primary ? QString() : name, crl::guard(box, [=] {
			*loading = false;
			box->showToast(primary
				? tr::lng_sg_dns_primary_cleared(tr::now)
				: tr::lng_sg_dns_primary_set(tr::now, lt_name, name));
		}), crl::guard(box, [=](QString type) {
			*loading = false;
			box->showToast(ErrorText(type));
		}));
	};
	const auto sell = [=] {
		const auto mine = dns->findMine(name);
		if (!mine) {
			box->showToast(tr::lng_sg_dns_error_not_owner(tr::now));
		} else if (!mine->minted) {
			box->showToast(tr::lng_sg_dns_transfer_minting(tr::now));
		} else {
			window->show(Box(SaleBox, dns, name));
		}
	};

	const auto fill = [=] {
		body->clear();
		const auto mine = dns->findMine(name);
		if (!mine) {
			AddText(
				body,
				(dns->myLoaded()
					? tr::lng_sg_dns_error_not_owner()
					: tr::lng_sg_dns_loading()),
				st::sgCardNote,
				QMargins(24, 12, 24, 12));
			body->resizeToWidth(container->width());
			return;
		}
		const auto domain = *mine;
		const auto listing = dns->myListing(name);

		const auto info = AddCard(body, 0, 6);
		AddInfoRow(
			info,
			tr::lng_sg_dns_status(),
			(domain.minted
				? tr::lng_sg_dns_status_ready()
				: tr::lng_sg_dns_status_minting()));
		if (domain.minted) {
			const auto own = domain.walletAddress.isEmpty()
				|| (domain.walletAddress == domain.ownerAddress);
			AddInfoRow(
				info,
				tr::lng_sg_dns_wallet_record(),
				(own
					? tr::lng_sg_dns_wallet_own()
					: rpl::single(ShortAddress(domain.walletAddress))),
				[=] { window->show(Box(WalletRecordBox, dns, name)); });
		}
		if (listing) {
			AddInfoRow(
				info,
				tr::lng_sg_dns_on_sale(),
				rpl::single(GramText(listing->price)));
		} else if (domain.purchasedAt) {
			AddInfoRow(
				info,
				tr::lng_sg_dns_purchased(),
				rpl::single(DateText(domain.purchasedAt)));
		}
		if (!domain.itemAddress.isEmpty()) {
			const auto address = domain.itemAddress;
			AddInfoRow(
				info,
				tr::lng_sg_dns_address(),
				rpl::single(ShortAddress(address)),
				[=] {
					QGuiApplication::clipboard()->setText(address);
					box->showToast(tr::lng_sg_dns_address_copied(tr::now));
				});
		}

		const auto actions = AddCard(body);
		Settings::AddButtonWithIcon(
			actions,
			(domain.primary
				? tr::lng_sg_dns_unset_primary()
				: tr::lng_sg_dns_set_primary()),
			st::settingsButton,
			{ domain.primary ? &st::menuIconUnpin : &st::menuIconPin }
		)->setClickedCallback(togglePrimary);
		Settings::AddButtonWithLabel(
			actions,
			tr::lng_sg_dns_sell(),
			rpl::single(listing ? GramText(listing->price) : QString()),
			st::settingsButton,
			{ &st::menuIconTagSell }
		)->setClickedCallback(sell);
		body->resizeToWidth(container->width());
	};
	fill();
	dns->updates() | rpl::on_next(fill, box->lifetime());
	if (!dns->myLoaded()) {
		dns->loadMy();
	}
	dns->loadMyListings();
}

void ShowDomainBox(
		not_null<Window::SessionController*> window,
		not_null<Controller*> dns,
		const QString &name) {
	window->show(Box(DomainBox, window, dns, FullName(name)));
}

void DomainsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> window) {
	const auto session = &window->session();
	const auto dns = Instance(session);
	dns->setShow(window->uiShow());
	SetupBox(box);
	box->setTitle(tr::lng_sg_dns_title());
	box->setWidth(st::boxWideWidth);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });

	struct State {
		base::Timer timer;
		QString name; // The normalized name in the field.
		bool checking = false;
		std::optional<Check> result;
		QString error;
		mtpRequestId requestId = 0;
		int seq = 0;
		rpl::variable<QString> buyText;
		std::vector<Listing> listings;
		int listingsCount = 0;
		bool listingsLoading = false;
		bool listingsLoaded = false;
		bool listingsFailed = false;
		Fn<void()> fillSale;
		Fn<void()> moreSale;
		Fn<void()> reloadSale;
	};
	const auto state = box->lifetime().make_state<State>();
	const auto container = box->verticalLayout();

	AddCardNote(container, tr::lng_sg_dns_intro());

	// The search: «[ name ] .ton», the state line and «Купить за».
	const auto searchCard = AddCard(container, 0, 6);
	const auto fieldRow = searchCard->add(
		object_ptr<Ui::RpWidget>(searchCard),
		QMargins());
	const auto field = Ui::CreateChild<Ui::InputField>(
		fieldRow,
		Mokagram::BoxParts::CardField(st::defaultInputField),
		tr::lng_sg_dns_search_hint(),
		QString());
	field->setMaxLength(kMaxName + int(Suffix().size()));
	const auto suffix = Ui::CreateChild<Ui::FlatLabel>(
		fieldRow,
		Suffix(),
		st::sgDnsSuffix);
	rpl::combine(
		fieldRow->widthValue(),
		field->heightValue()
	) | rpl::on_next([=](int width, int height) {
		const auto right = 16;
		const auto available = std::max(
			width - suffix->width() - right,
			1);
		field->resizeToWidth(available);
		field->moveToLeft(0, 0);
		suffix->moveToLeft(
			width - suffix->width() - right,
			(height - suffix->height()) / 2);
		if (fieldRow->height() != height) {
			fieldRow->resize(width, height);
		}
	}, fieldRow->lifetime());
	box->setFocusCallback([=] {
		field->setFocusFast();
	});

	const auto stateLabel = searchCard->add(
		object_ptr<Ui::FlatLabel>(searchCard, QString(), st::sgDnsState),
		kInfoMargins);

	const auto buyWrap = container->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			container,
			object_ptr<Ui::VerticalLayout>(container)));

	const auto bind = [=] {
		auto text = QString();
		auto color = std::optional<QColor>();
		auto buyable = false;
		const auto &result = state->result;
		const auto mine = result ? dns->findMine(result->name) : nullptr;
		if (state->name.isEmpty()) {
			text = tr::lng_sg_dns_search_empty(tr::now);
		} else if (state->checking || (!result && state->error.isEmpty())) {
			text = tr::lng_sg_dns_checking(tr::now);
		} else if (result && result->available) {
			text = tr::lng_sg_dns_available(tr::now);
			color = st::boxTextFgGood->c;
			buyable = true;
			state->buyText = tr::lng_sg_dns_buy_for(
				tr::now,
				lt_amount,
				GramText(result->price));
		} else if (result
			&& result->listing
			&& !mine
			&& result->listing->sellerId != session->userId()) {
			// Taken, but its owner sells it: buy at the listing price.
			text = tr::lng_sg_dns_on_sale(tr::now);
			color = st::windowActiveTextFg->c;
			buyable = true;
			state->buyText = tr::lng_sg_dns_buy_for(
				tr::now,
				lt_amount,
				GramText(result->listing->price));
		} else if (result) {
			text = mine
				? tr::lng_sg_dns_state_yours(tr::now)
				: tr::lng_sg_dns_state_taken(tr::now);
			color = mine
				? st::windowActiveTextFg->c
				: st::boxTextFgError->c;
		} else {
			text = (state->error == u"TONDNS_NAME_INVALID"_q)
				? tr::lng_sg_dns_state_invalid(tr::now)
				: ErrorText(state->error);
			color = st::boxTextFgError->c;
		}
		stateLabel->setText(text);
		stateLabel->setTextColorOverride(color);
		buyWrap->toggle(buyable, anim::type::normal);
	};

	const auto runCheck = [=] {
		const auto name = state->name;
		if (name.isEmpty()) {
			return;
		}
		dns->cancel(base::take(state->requestId));
		state->checking = true;
		state->result = std::nullopt;
		state->error = QString();
		bind();
		const auto seq = ++state->seq;
		state->requestId = dns->check(name, crl::guard(box, [=](Check check) {
			if (seq != state->seq) {
				return;
			}
			state->requestId = 0;
			state->checking = false;
			state->result = std::move(check);
			bind();
		}), crl::guard(box, [=](QString type) {
			if (seq != state->seq) {
				return;
			}
			state->requestId = 0;
			state->checking = false;
			state->error = type.isEmpty() ? u"UNKNOWN"_q : type;
			bind();
		}));
	};
	state->timer.setCallback(runCheck);

	field->changes() | rpl::on_next([=] {
		const auto text = field->getLastText();
		auto lower = text.trimmed().toLower();
		if (lower.startsWith('@')) {
			lower = lower.mid(1);
		}
		if (lower.endsWith(Suffix())) {
			lower.chop(Suffix().size());
		}
		auto filtered = QString();
		for (const auto &ch : lower) {
			const auto c = ch.unicode();
			if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
				filtered += ch;
			}
		}
		filtered = filtered.left(kMaxName);
		if (filtered != text) {
			field->setText(filtered);
			field->setCursorPosition(filtered.size());
		}
		if (filtered == state->name) {
			return;
		}
		state->name = filtered;
		dns->cancel(base::take(state->requestId));
		++state->seq;
		state->checking = false;
		state->result = std::nullopt;
		state->error = QString();
		if (filtered.isEmpty()) {
			state->timer.cancel();
		} else {
			state->timer.callOnce(kCheckDelay);
		}
		bind();
	}, field->lifetime());

	const auto onBuy = [=] {
		const auto &result = state->result;
		if (!result) {
			return;
		} else if (result->available) {
			window->show(Box(
				BuyBox,
				window,
				dns,
				result->name,
				result->price,
				crl::guard(box, runCheck)));
		} else if (result->listing) {
			window->show(Box(
				BuyListedBox,
				window,
				dns,
				*result->listing,
				crl::guard(box, runCheck)));
		}
	};
	AddMainButton(buyWrap->entity(), state->buyText.value(), onBuy, 4);
	buyWrap->toggle(false, anim::type::instant);

	// «Мои домены».
	const auto myCard = AddCard(container);
	AddCardTitle(myCard, tr::lng_sg_dns_my());
	const auto myList = myCard->add(
		object_ptr<Ui::VerticalLayout>(myCard));
	const auto fillMy = [=] {
		myList->clear();
		const auto &list = dns->my();
		if (list.empty()) {
			AddText(
				myList,
				(!dns->myLoaded()
					? (dns->myFailed()
						? tr::lng_sg_dns_my_error()
						: tr::lng_sg_dns_loading())
					: tr::lng_sg_dns_my_empty()),
				st::sgCardNote,
				kInfoMargins);
			if (dns->myLoaded()) {
				AddText(
					myList,
					tr::lng_sg_dns_my_empty_hint(),
					st::sgCardNote,
					kInfoMargins);
			}
		} else {
			for (const auto &domain : list) {
				const auto name = domain.name;
				Settings::AddButtonWithLabel(
					myList,
					rpl::single(name),
					rpl::single(StatusText(dns, domain)),
					st::settingsButton,
					{ &st::menuIconLink }
				)->setClickedCallback([=] {
					ShowDomainBox(window, dns, name);
				});
			}
		}
		myList->resizeToWidth(myCard->width());
	};

	// «Продаются»: the resale showcase.
	const auto saleCard = AddCard(container);
	AddCardTitle(saleCard, tr::lng_sg_dns_for_sale());
	const auto saleList = saleCard->add(
		object_ptr<Ui::VerticalLayout>(saleCard));
	state->fillSale = [=] {
		saleList->clear();
		if (state->listings.empty()) {
			AddText(
				saleList,
				(state->listingsFailed
					? tr::lng_sg_dns_error_unavailable()
					: !state->listingsLoaded
					? tr::lng_sg_dns_loading()
					: tr::lng_sg_dns_for_sale_empty()),
				st::sgCardNote,
				kInfoMargins);
		}
		for (const auto &listing : state->listings) {
			Settings::AddButtonWithLabel(
				saleList,
				rpl::single(listing.name),
				rpl::single(GramText(listing.price)),
				st::settingsButton,
				{ &st::menuIconTagSell }
			)->setClickedCallback([=] {
				if (listing.sellerId == session->userId()) {
					ShowDomainBox(window, dns, listing.name);
				} else {
					window->show(Box(
						BuyListedBox,
						window,
						dns,
						listing,
						crl::guard(box, [=] { state->reloadSale(); })));
				}
			});
		}
		if (state->listingsLoaded
			&& !state->listingsLoading
			&& int(state->listings.size()) < state->listingsCount) {
			Settings::AddButtonWithIcon(
				saleList,
				tr::lng_sg_dns_for_sale_more(),
				st::settingsButtonNoIcon
			)->setClickedCallback([=] {
				state->moreSale();
			});
		}
		saleList->resizeToWidth(saleCard->width());
	};
	state->moreSale = [=] {
		if (state->listingsLoading) {
			return;
		}
		state->listingsLoading = true;
		dns->loadListings(int(state->listings.size()), crl::guard(box, [=](
				std::vector<Listing> list,
				int count) {
			state->listingsLoading = false;
			for (auto &listing : list) {
				state->listings.push_back(std::move(listing));
			}
			state->listingsCount = count;
			state->fillSale();
		}), crl::guard(box, [=] {
			state->listingsLoading = false;
			state->fillSale();
		}));
	};
	// Loads the first page again (after a purchase or on open).
	state->reloadSale = [=] {
		state->listingsLoading = true;
		dns->loadListings(0, crl::guard(box, [=](
				std::vector<Listing> list,
				int count) {
			state->listingsLoading = false;
			state->listingsLoaded = true;
			state->listingsFailed = false;
			state->listings = std::move(list);
			state->listingsCount = count;
			state->fillSale();
		}), crl::guard(box, [=] {
			state->listingsLoading = false;
			state->listingsFailed = !state->listingsLoaded;
			state->fillSale();
		}));
	};

	bind();
	fillMy();
	state->fillSale();
	dns->updates() | rpl::on_next([=] {
		bind();
		fillMy();
	}, box->lifetime());
	dns->loadMy();
	dns->loadMyListings();
	state->reloadSale();
}

} // namespace

QString Normalize(const QString &input) {
	auto s = input.trimmed().toLower();
	if (s.startsWith('@')) {
		s = s.mid(1);
	}
	if (s.endsWith(Suffix())) {
		s.chop(Suffix().size());
	}
	if (s.isEmpty() || s.size() > kMaxName) {
		return QString();
	}
	for (const auto &ch : s) {
		const auto c = ch.unicode();
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
			return QString();
		}
	}
	return s;
}

void ShowDomainsBox(not_null<Window::SessionController*> controller) {
	controller->show(Box(DomainsBox, controller));
}

} // namespace StaticGram::Domains
