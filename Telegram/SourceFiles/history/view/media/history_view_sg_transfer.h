/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;
class PeerData;

namespace Data {
struct CreditsHistoryEntry;
} // namespace Data

namespace Main {
class SessionShow;
} // namespace Main

namespace HistoryView {

class Element;
class Media;

// StaticGram: the gold Stars card (Android WalletActionLayout.setStars) for
// sgcard.messageActionStarsTransfer and for the Stars grants and giveaway
// prizes (messageActionGiftStars / messageActionPrizeStars), with the
// comment under it. The item has HistoryServiceGramTransfer with sgStars.
[[nodiscard]] std::unique_ptr<Media> CreateSgStarsTransferMedia(
	not_null<Element*> parent,
	Element *replacing);

// What a GRAM / Stars card says, from the message alone: the upstream
// wallet engine is never used (AppConfig::walletAvailable() is false).
void ShowSgTransferDetails(
	std::shared_ptr<Main::SessionShow> show,
	not_null<HistoryItem*> item);

// StaticGram: a Stars transfer row of the «Деньги» history (sgcard.sendStars)
// opens the same receipt as the chat card, not the upstream Stars box.
[[nodiscard]] bool IsSgStarsTransferEntry(
	const Data::CreditsHistoryEntry &entry);
void ShowSgStarsTransferReceipt(
	std::shared_ptr<Main::SessionShow> show,
	not_null<PeerData*> self,
	const Data::CreditsHistoryEntry &entry);

// gramsrv marks a captioned admin grant sent as a transfer action with this
// transaction id prefix (tlprofile.GrantTransferIDPrefix).
[[nodiscard]] bool IsSgGrantTransactionId(const QString &id);

} // namespace HistoryView
