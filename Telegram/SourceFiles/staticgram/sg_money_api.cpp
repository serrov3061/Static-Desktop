/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_money_api.h"

#include "api/api_premium.h" // Api::FromTL.
#include "apiwrap.h"
#include "base/random.h"
#include "data/components/credits.h"
#include "data/data_credits.h"
#include "data/data_document.h"
#include "data/data_session.h"
#include "data/data_star_gift.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "lang/lang_tag.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "ui/controls/ton_common.h" // TonAmountSeparator.
#include "ui/text/format_values.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace StaticGram::Money {
namespace {

constexpr auto kCardsCacheTimeout = crl::time(30'000);
constexpr auto kNftsLimit = 30;
constexpr auto kDefaultRubPerUsd = 90.;
constexpr auto kRatesRefreshTimeout = crl::time(6 * 3600 * 1000);
constexpr auto kRatesRetryTimeout = crl::time(10 * 60 * 1000);
constexpr auto kCompactFrom = int64(1'000'000);

struct CardsEntry {
	rpl::variable<Cards> value;
	crl::time loaded = 0;
	mtpRequestId requestId = 0;
};

struct SessionState {
	base::flat_map<UserId, std::unique_ptr<CardsEntry>> cards;
	rpl::variable<int64> gram = -1; // Unknown yet.
	mtpRequestId gramRequestId = 0;
	rpl::lifetime lifetime;
};

struct Rates {
	float64 rubPerUsd = kDefaultRubPerUsd;
	crl::time requested = 0;
	bool fetched = false;
	bool fetching = false;
	rpl::event_stream<> updates;
};

[[nodiscard]] Rates &GlobalRates() {
	static auto result = Rates();
	return result;
}

void RequestGram(not_null<Main::Session*> session);

[[nodiscard]] base::flat_map<
	not_null<Main::Session*>,
	std::unique_ptr<SessionState>> &States() {
	static auto result = base::flat_map<
		not_null<Main::Session*>,
		std::unique_ptr<SessionState>>();
	return result;
}

[[nodiscard]] not_null<SessionState*> State(
		not_null<Main::Session*> session) {
	auto &states = States();
	const auto i = states.find(session);
	if (i != end(states)) {
		return i->second.get();
	}
	const auto result = states.emplace(
		session,
		std::make_unique<SessionState>()).first->second.get();
	session->lifetime().add([=] {
		States().erase(session);
	});
	// The spendable part of GRAM comes in updateStarsBalance (ton),
	// any change of it means the full GRAM balance changed too.
	session->credits().tonBalanceValue(
	) | rpl::skip(1) | rpl::on_next([=] {
		RequestGram(session);
	}, result->lifetime);
	return result;
}

void RequestGram(not_null<Main::Session*> session) {
	const auto state = State(session);
	if (state->gramRequestId) {
		return;
	}
	state->gramRequestId = session->api().request(
		MTPgramwallet_GetBalance()
	).done([=](const MTPgramwallet_Balance &result) {
		const auto state = State(session);
		state->gramRequestId = 0;
		const auto &data = result.data();
		state->gram = int64(data.vwallet().v) + int64(data.vgram().v);
	}).fail([=] {
		State(session)->gramRequestId = 0;
	}).send();
}

[[nodiscard]] Kind ParseKind(int value) {
	return (value == int(Kind::Stars)) ? Kind::Stars : Kind::Gram;
}

[[nodiscard]] DocumentData *StickerDocument(
		not_null<Main::Session*> session,
		const MTPDsgcard_sticker &data) {
	if (const auto gift = data.vgift()) {
		if (const auto parsed = Api::FromTL(session, *gift)) {
			return parsed->unique
				? parsed->unique->model.document.get()
				: parsed->document.get();
		}
	} else if (const auto document = data.vdocument()) {
		if (document->type() == mtpc_document) {
			return session->data().processDocument(*document).get();
		}
	}
	return nullptr;
}

[[nodiscard]] Cards ParseCards(
		not_null<Main::Session*> session,
		const MTPsgcard_Cards &result) {
	const auto &data = result.data();
	session->data().processUsers(data.vusers());
	auto cards = Cards{
		.purchased = data.is_purchased(),
		.loaded = true,
	};
	for (const auto &card : data.vcards().v) {
		const auto &fields = card.data();
		auto entry = Card{
			.kind = ParseKind(fields.vkind().v),
			.balance = int64(fields.vbalance().v),
		};
		if (const auto background = fields.vbackground()) {
			if (const auto gift = Api::FromTL(session, *background)) {
				entry.background = gift->unique;
			}
		}
		for (const auto &sticker : fields.vstickers().v) {
			if (int(entry.stickers.size()) >= kMaxStickers) {
				break;
			}
			const auto &values = sticker.data();
			const auto document = StickerDocument(session, values);
			if (!document) {
				continue;
			}
			entry.stickers.push_back({
				.document = document,
				.x = std::clamp(values.vx().v, 0., 1.),
				.y = std::clamp(values.vy().v, 0., 1.),
				.scale = std::clamp(values.vscale().v, 0.05, 1.),
				.rotation = std::clamp(values.vrotation().v, -360., 360.),
			});
		}
		const auto already = ranges::find(
			cards.list,
			entry.kind,
			&Card::kind);
		if (already == end(cards.list)) {
			cards.list.push_back(std::move(entry));
		}
	}
	return cards;
}

[[nodiscard]] not_null<CardsEntry*> Entry(not_null<UserData*> user) {
	const auto state = State(&user->session());
	const auto id = peerToUser(user->id);
	auto i = state->cards.find(id);
	if (i == end(state->cards)) {
		i = state->cards.emplace(id, std::make_unique<CardsEntry>()).first;
	}
	return i->second.get();
}

void RequestCards(not_null<UserData*> user, bool force) {
	const auto entry = Entry(user);
	const auto now = crl::now();
	if (entry->requestId
		|| (!force
			&& entry->loaded
			&& (now - entry->loaded) < kCardsCacheTimeout)) {
		return;
	}
	const auto session = &user->session();
	const auto id = peerToUser(user->id);
	const auto find = [=]() -> CardsEntry* {
		const auto state = State(session);
		const auto i = state->cards.find(id);
		return (i != end(state->cards)) ? i->second.get() : nullptr;
	};
	entry->requestId = session->api().request(MTPsgcard_GetCards(
		user->inputUser()
	)).done([=](const MTPsgcard_Cards &result) {
		if (const auto entry = find()) {
			entry->requestId = 0;
			entry->loaded = crl::now();
			entry->value = ParseCards(session, result);
		}
	}).fail([=] {
		if (const auto entry = find()) {
			entry->requestId = 0;
			auto failed = entry->value.current();
			if (!failed.loaded) {
				failed.failed = true;
				entry->value = std::move(failed);
			}
		}
	}).send();
}

void RequestRates() {
	auto &rates = GlobalRates();
	const auto now = crl::now();
	if (rates.fetching
		|| (rates.requested
			&& (now - rates.requested) < (rates.fetched
				? kRatesRefreshTimeout
				: kRatesRetryTimeout))) {
		return;
	}
	const auto app = QCoreApplication::instance();
	if (!app) {
		return;
	}
	static const auto manager = new QNetworkAccessManager(app);
	rates.fetching = true;
	rates.requested = now;
	auto request = QNetworkRequest(QUrl(
		u"https://api.coinbase.com/v2/exchange-rates?currency=USD"_q));
	request.setTransferTimeout(15'000);
	const auto reply = manager->get(request);
	QObject::connect(reply, &QNetworkReply::finished, manager, [=] {
		reply->deleteLater();
		auto &rates = GlobalRates();
		rates.fetching = false;
		if (reply->error() != QNetworkReply::NoError) {
			return;
		}
		const auto document = QJsonDocument::fromJson(reply->readAll());
		const auto value = document.object().value(u"data"_q).toObject(
		).value(u"rates"_q).toObject().value(u"RUB"_q);
		const auto rub = value.isString()
			? value.toString().toDouble()
			: value.toDouble();
		if (rub > 1. && rub < 100'000.) {
			rates.rubPerUsd = rub;
			rates.fetched = true;
			rates.updates.fire({});
		}
	});
}

[[nodiscard]] QString Compact(int64 units) {
	static const auto kUnits = std::array<const char*, 6>{
		"", "K", "M", "B", "T", "Q"
	};
	const auto negative = (units < 0);
	auto value = negative ? -units : units;
	auto base = int64(1);
	auto unit = 0;
	while (value / base >= 1000 && unit + 1 < int(kUnits.size())) {
		base *= 1000;
		++unit;
	}
	const auto whole = value / base;
	auto result = QString::number(whole);
	if (whole < 100 && base > 1) {
		const auto decimal = ((value % base) * 10) / base;
		if (decimal > 0) {
			result += '.' + QString::number(decimal);
		}
	}
	return (negative ? u"-"_q : QString())
		+ result
		+ QString::fromLatin1(kUnits[unit]);
}

[[nodiscard]] float64 UsdPerUnit(
		not_null<Main::Session*> session,
		Kind kind) {
	const auto &config = session->appConfig();
	return (kind == Kind::Gram)
		? config.get<double>(u"ton_usd_rate"_q, 3.)
		: (config.get<double>(u"stars_usd_sell_rate_x1000"_q, 2000.)
			/ 100'000.);
}

} // namespace

const Card *Cards::find(Kind kind) const {
	const auto i = ranges::find(list, kind, &Card::kind);
	return (i != end(list)) ? &*i : nullptr;
}

rpl::producer<Cards> CardsValue(not_null<UserData*> user) {
	RequestCards(user, false);
	return Entry(user)->value.value();
}

void RefreshCards(not_null<UserData*> user) {
	RequestCards(user, true);
}

rpl::producer<int64> GramBalanceValue(not_null<Main::Session*> session) {
	const auto state = State(session);
	RequestGram(session);
	return state->gram.value() | rpl::filter([](int64 value) {
		return value >= 0;
	});
}

rpl::producer<int64> StarsBalanceValue(not_null<Main::Session*> session) {
	const auto credits = &session->credits();
	credits->load();
	return rpl::combine(
		credits->loadedValue(),
		credits->balanceValue()
	) | rpl::filter([](bool loaded, CreditsAmount) {
		return loaded;
	}) | rpl::map([](bool, CreditsAmount amount) {
		return amount.whole();
	});
}

rpl::producer<int64> BalanceValue(
		not_null<Main::Session*> session,
		Kind kind) {
	return (kind == Kind::Gram)
		? GramBalanceValue(session)
		: StarsBalanceValue(session);
}

void RefreshBalances(not_null<Main::Session*> session) {
	session->credits().load(true);
	session->credits().tonLoad(true);
	RequestGram(session);
	RequestCards(session->user(), true);
}

QString FormatGram(int64 nanos) {
	const auto negative = (nanos < 0);
	const auto value = negative ? -nanos : nanos;
	const auto whole = value / kNanosInOne;
	if (whole >= kCompactFrom) {
		return (negative ? u"-"_q : QString()) + Compact(whole);
	}
	const auto rest = value % kNanosInOne;
	// At most 2 decimals (4 for dust), truncated, trailing zeros trimmed.
	const auto digits = (value > 0 && value < kNanosInOne / 100) ? 4 : 2;
	auto divider = kNanosInOne;
	for (auto i = 0; i != digits; ++i) {
		divider /= 10;
	}
	auto fraction = QString::number(rest / divider).rightJustified(
		digits,
		QChar('0'));
	while (fraction.endsWith(QChar('0'))) {
		fraction.chop(1);
	}
	auto result = Lang::FormatCountDecimal(whole);
	if (!fraction.isEmpty()) {
		result += Ui::TonAmountSeparator() + fraction;
	}
	return (negative ? u"-"_q : QString()) + result;
}

QString FormatStars(int64 stars) {
	return (std::abs(stars) < kCompactFrom)
		? Lang::FormatCountDecimal(stars)
		: Compact(stars);
}

QString FormatAmount(Kind kind, int64 amount) {
	return (kind == Kind::Gram) ? FormatGram(amount) : FormatStars(amount);
}

QString CardNumber(Kind kind, not_null<UserData*> user) {
	const auto raw = ((kind == Kind::Stars) ? u"STAR"_q : u"GRAM"_q)
		+ QString::number(peerToUser(user->id).bare);
	auto result = QString();
	result.reserve(raw.size() + raw.size() / 4);
	for (auto i = 0; i != raw.size(); ++i) {
		if (i > 0 && !(i % 4)) {
			result.append(QChar(' '));
		}
		result.append(raw[i]);
	}
	return result;
}

QString FiatText(
		not_null<Main::Session*> session,
		Kind kind,
		int64 amount) {
	RequestRates();
	if (amount <= 0) {
		return QString();
	}
	const auto perUnit = UsdPerUnit(session, kind);
	if (!(perUnit > 0.) || std::isinf(perUnit)) {
		return QString();
	}
	const auto units = (kind == Kind::Gram)
		? (amount / float64(kNanosInOne))
		: float64(amount);
	const auto rub = units * perUnit * GlobalRates().rubPerUsd;
	if (!(rub >= 0.) || rub > 1e15) {
		return QString();
	}
	return QString::fromUtf8("\xE2\x89\x88 ") + Ui::FillAmountAndCurrency(
		int64(std::round(rub * 100.)),
		u"RUB"_q);
}

rpl::producer<QString> FiatValue(
		not_null<Main::Session*> session,
		Kind kind,
		rpl::producer<int64> amount) {
	return rpl::combine(
		std::move(amount),
		rpl::single(rpl::empty) | rpl::then(GlobalRates().updates.events())
	) | rpl::map([=](int64 value, auto) {
		return FiatText(session, kind, value);
	});
}

QString NewRequestKey() {
	auto result = u"sg"_q;
	for (auto i = 0; i != 2; ++i) {
		result += QString::number(
			base::RandomValue<uint64>(),
			16).rightJustified(16, QChar('0'));
	}
	return result;
}

void Send(
		const SendRequest &request,
		Fn<void(SendResult)> done,
		Fn<void(QString)> fail) {
	const auto user = request.to;
	const auto session = &user->session();
	const auto failed = [=](const MTP::Error &error) {
		fail(error.type());
	};
	if (request.kind == Kind::Gram) {
		session->api().request(MTPgramwallet_Send(
			MTP_long(peerToUser(user->id).bare),
			MTP_long(request.amount),
			MTP_string(request.comment),
			MTP_string(request.requestKey)
		)).done([=](const MTPgramwallet_Result &result) {
			const auto &data = result.data();
			RefreshBalances(session);
			RefreshCards(user);
			done({
				.fee = int64(data.vfee().v),
				.replayed = mtpIsTrue(data.vreplayed()),
			});
		}).fail(failed).send();
	} else {
		session->api().request(MTPsgcard_SendStars(
			user->inputUser(),
			MTP_long(request.amount),
			MTP_string(request.comment),
			MTP_string(request.requestKey)
		)).done([=](const MTPsgcard_StarsResult &result) {
			const auto &data = result.data();
			RefreshBalances(session);
			RefreshCards(user);
			done({ .replayed = mtpIsTrue(data.vreplayed()) });
		}).fail(failed).send();
	}
}

QString ErrorText(const QString &type) {
	if (type == u"BALANCE_TOO_LOW"_q) {
		return tr::lng_sg_money_error_balance(tr::now);
	} else if (type == u"USER_GIFT_UNAVAILABLE"_q
		|| type == u"PEER_ID_INVALID"_q
		|| type == u"USER_ID_INVALID"_q) {
		return tr::lng_sg_money_error_recipient(tr::now);
	} else if (type == u"USER_IS_BOT"_q) {
		return tr::lng_sg_money_error_bot(tr::now);
	} else if (type == u"STARS_AMOUNT_INVALID"_q) {
		return tr::lng_sg_money_error_stars_amount(
			tr::now,
			lt_max,
			Lang::FormatCountDecimal(kMaxStars));
	} else if (type == u"WALLET_CARD_COMMENT_TOO_LONG"_q) {
		return tr::lng_sg_money_error_comment(
			tr::now,
			lt_max,
			QString::number(kMaxComment));
	} else if (type == u"INPUT_REQUEST_INVALID"_q) {
		return tr::lng_sg_money_error_amount(tr::now);
	} else if (type.isEmpty() || type == u"INTERNAL"_q) {
		return tr::lng_sg_money_error_unknown(tr::now);
	}
	return tr::lng_sg_money_error_code(tr::now, lt_code, type);
}

void LoadNfts(
		not_null<Main::Session*> session,
		const QString &offset,
		Fn<void(NftSlice)> done,
		Fn<void()> fail) {
	session->api().request(MTPwallet_GetNfts(
		MTP_string(offset),
		MTP_int(kNftsLimit)
	)).done([=](const MTPwallet_NftItems &result) {
		const auto &data = result.data();
		auto slice = NftSlice{
			.next = qs(data.vnext_offset().value_or_empty()),
		};
		for (const auto &item : data.vitems().v) {
			if (const auto gift = item.data().vgift()) {
				if (auto parsed = Api::FromTL(session, *gift)) {
					slice.gifts.push_back(std::move(*parsed));
				}
			}
		}
		done(std::move(slice));
	}).fail([=] {
		fail();
	}).send();
}

} // namespace StaticGram::Money
