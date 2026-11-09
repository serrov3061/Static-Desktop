/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Data {
struct StarGift;
struct UniqueGift;
} // namespace Data

namespace Main {
class Session;
} // namespace Main

class DocumentData;
class UserData;

// StaticGram «Money» (Android ui/Wallet/SGCardsController, SGFiat,
// SGAmounts): GRAM and Stars balances, wallet cards (sgcard.*) and the
// transfers (gramwallet.send, sgcard.sendStars) of our own server. The
// upstream client-side TON wallet is not used.
namespace StaticGram::Money {

enum class Kind {
	Gram = 0,
	Stars = 1,
};

inline constexpr auto kNanosInOne = int64(1'000'000'000);
inline constexpr auto kMaxStars = int64(1'000'000);
inline constexpr auto kMaxComment = 256;
inline constexpr auto kMaxStickers = 12;

struct CardSticker {
	not_null<DocumentData*> document;
	float64 x = 0.5; // Centre, 0..1 of the card.
	float64 y = 0.5;
	float64 scale = 0.2; // Of the card width.
	float64 rotation = 0.; // Degrees.
};

struct Card {
	Kind kind = Kind::Gram;
	std::shared_ptr<Data::UniqueGift> background;
	std::vector<CardSticker> stickers;
	int64 balance = 0; // Nanoton for GRAM (wallet + gram), Stars as is.
};

struct Cards {
	std::vector<Card> list;
	bool purchased = false;
	bool loaded = false;
	bool failed = false;

	[[nodiscard]] const Card *find(Kind kind) const;
};

// sgcard.getCards for any user, cached for 30 seconds.
[[nodiscard]] rpl::producer<Cards> CardsValue(not_null<UserData*> user);
void RefreshCards(not_null<UserData*> user);

// Own balances: GRAM is wallet + gram of gramwallet.getBalance, Stars come
// from the credits component (updateStarsBalance keeps them fresh).
[[nodiscard]] rpl::producer<int64> GramBalanceValue(
	not_null<Main::Session*> session);
[[nodiscard]] rpl::producer<int64> StarsBalanceValue(
	not_null<Main::Session*> session);
[[nodiscard]] rpl::producer<int64> BalanceValue(
	not_null<Main::Session*> session,
	Kind kind);
void RefreshBalances(not_null<Main::Session*> session);

// "94.4K", "1,234.5" (GRAM in nanoton) or "1 234" (Stars).
[[nodiscard]] QString FormatGram(int64 nanos);
[[nodiscard]] QString FormatStars(int64 stars);
[[nodiscard]] QString FormatAmount(Kind kind, int64 amount);
// "GRAM 1780 2432 00" / "STAR 1780 2432 00".
[[nodiscard]] QString CardNumber(Kind kind, not_null<UserData*> user);

// "≈ 245,90 ₽", empty for a zero amount. The rouble rate is fetched once
// in a while (like the Android fallback), a built-in rate is used until.
[[nodiscard]] rpl::producer<QString> FiatValue(
	not_null<Main::Session*> session,
	Kind kind,
	rpl::producer<int64> amount);
[[nodiscard]] QString FiatText(
	not_null<Main::Session*> session,
	Kind kind,
	int64 amount);

struct SendRequest {
	Kind kind = Kind::Gram;
	not_null<UserData*> to;
	int64 amount = 0;
	QString comment;
	QString requestKey;
};
struct SendResult {
	int64 fee = 0;
	bool replayed = false;
};
// The request key makes a retry with the same parameters idempotent.
[[nodiscard]] QString NewRequestKey();
void Send(
	const SendRequest &request,
	Fn<void(SendResult)> done,
	Fn<void(QString)> fail);
[[nodiscard]] QString ErrorText(const QString &type);

struct NftSlice {
	std::vector<Data::StarGift> gifts;
	QString next;
};
// wallet.getNfts: the collectibles on the custodial chain wallet.
void LoadNfts(
	not_null<Main::Session*> session,
	const QString &offset,
	Fn<void(NftSlice)> done,
	Fn<void()> fail);

} // namespace StaticGram::Money
