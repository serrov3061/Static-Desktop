// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 277d403b
// Mokagram: colored settings row icons from a custom emoji set.
#include "mokagram/settings/mokagram_row_icons.h"

#include "apiwrap.h"
#include "core/application.h"
#include "data/data_document.h"
#include "data/data_session.h"
#include "data/stickers/data_custom_emoji.h"
#include "main/main_session.h"
#include "ui/text/text_custom_emoji.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#include <rpl/event_stream.h>

namespace Mokagram::Settings {
namespace {

// Icons from the custom emoji set: a document is found by its plain emoji.
class PackIcons final {
public:
	static PackIcons &Instance() {
		static auto result = PackIcons();
		return result;
	}

	void ensure(not_null<Main::Session*> session) {
		if (_session == session || _requestId) {
			return;
		}
		_session = session;
		_requestId = session->api().request(MTPmessages_GetStickerSet(
			MTP_inputStickerSetShortName(
				MTP_string(QString::fromLatin1(kRowIconsSet))),
			MTP_int(0)
		)).done([=](const MTPmessages_StickerSet &set) {
			_requestId = 0;
			set.match([&](const MTPDmessages_stickerSet &data) {
				for (const auto &doc : data.vdocuments().v) {
					const auto document = _session->data().processDocument(doc);
					if (const auto sticker = document->sticker()) {
						_docs.emplace(Normalize(sticker->alt), document);
					}
				}
				_updated.fire({});
			}, [](const auto &) {});
		}).fail([=] {
			_requestId = 0;
		}).send();
	}
	[[nodiscard]] rpl::producer<> updated() const {
		return _updated.events();
	}
	[[nodiscard]] std::unique_ptr<Ui::Text::CustomEmoji> makeAlt(
			const QString &alt,
			Fn<void()> repaint,
			int size) {
		const auto i = _docs.find(Normalize(alt));
		if (!_session || i == _docs.end()) {
			return nullptr;
		}
		return _session->data().customEmojiManager().create(
			i->second,
			std::move(repaint),
			// Isolated грузит кадры крупнее: значок остаётся чётким после обрезки.
			Data::CustomEmojiSizeTag::Isolated,
			size);
	}

private:
	[[nodiscard]] static QString Normalize(QString value) {
		return value.remove(QChar(0xFE0F)).trimmed();
	}

	Main::Session *_session = nullptr;
	mtpRequestId _requestId = 0;
	base::flat_map<QString, DocumentData*> _docs;
	rpl::event_stream<> _updated;

};

[[nodiscard]] const base::flat_map<const void*, QString> &RowIconMap() {
	static const auto map = base::flat_map<const void*, QString>{
		{ &st::menuIconProfile, QString::fromUtf8("👤") },
		{ &st::menuIconPhone, QString::fromUtf8("📞") },
		{ &st::menuIconUsername, QString::fromUtf8("🔗") },
		{ &st::settingsIconChatAutomation, QString::fromUtf8("🤖") },
		{ &st::menuIconNotifications, QString::fromUtf8("🔔") },
		{ &st::menuIconLock, QString::fromUtf8("🔑") },
		{ &st::menuIconChatBubble, QString::fromUtf8("💬") },
		{ &st::menuIconShowInFolder, QString::fromUtf8("📁") },
		{ &st::menuIconManage, QString::fromUtf8("⚙️") },
		{ &st::menuIconReport, QString::fromUtf8("❗️") },
		{ &st::menuIconUnmute, QString::fromUtf8("🎵") },
		{ &st::menuIconBotCommands, QString::fromUtf8("🔧") },
		{ &st::menuIconPalette, QString::fromUtf8("🖼") },
		{ &st::menuIconPowerUsage, QString::fromUtf8("💤") },
		{ &st::menuIconTranslate, QString::fromUtf8("🗣") },
		{ &st::menuIconTon, QString::fromUtf8("💰") },
		{ &st::menuIconShop, QString::fromUtf8("📈") },
		{ &st::menuIconGiftPremium, QString::fromUtf8("🎁") },
		{ &st::menuIconFaq, QString::fromUtf8("❔") },
		{ &st::menuIconEmojiObjects, QString::fromUtf8("ℹ️") },
		{ &st::menuIconShowInChat, QString::fromUtf8("👁") },
		{ &st::menuIconDiscussion, QString::fromUtf8("💬") },
		{ &st::menuIconCopy, QString::fromUtf8("📋") },
		{ &st::menuIconStickers, QString::fromUtf8("🙂") },
		{ &st::menuIconPermissions, QString::fromUtf8("🛡") },
		{ &st::menuIconDeleteAttention, QString::fromUtf8("🗑") },
		{ &st::menuIconChannel, QString::fromUtf8("📢") },
		{ &st::menuIconNetwork, QString::fromUtf8("🔗") },
		{ &st::menuIconGroups, QString::fromUtf8("👥") },
		{ &st::menuIconGroupReactions, QString::fromUtf8("💟") },
		{ &st::menuIconEmoji, QString::fromUtf8("😜") },
		{ &st::menuIconEdit, QString::fromUtf8("✏️") },
		{ &st::menuIconChangeColors, QString::fromUtf8("🎈") },
		{ &st::menuIconTTL, QString::fromUtf8("⏳") },
		{ &st::menuIconShortcut, QString::fromUtf8("▶️") },
		{ &st::menuIconShare, QString::fromUtf8("📤") },
		{ &st::menuIconReplace, QString::fromUtf8("↩️") },
		{ &st::menuIconRecoveryEmail, QString::fromUtf8("📑") },
		{ &st::menuIconPremium, QString::fromUtf8("⭐️") },
		{ &st::menuIconPin, QString::fromUtf8("📌") },
		{ &st::menuIconPhone, QString::fromUtf8("📞") },
		{ &st::menuIconNightMode, QString::fromUtf8("💤") },
		{ &st::menuIconLink, QString::fromUtf8("🔗") },
		{ &st::menuIconLeaveAttention, QString::fromUtf8("⛔️") },
		{ &st::menuIconIpAddress, QString::fromUtf8("📍") },
		{ &st::menuIconFont, QString::fromUtf8("✏️") },
		{ &st::menuIconDevices, QString::fromUtf8("🤳") },
		{ &st::menuIconDelete, QString::fromUtf8("🗑") },
		{ &st::menuIconBlock, QString::fromUtf8("🚫") },
		{ &st::menuIconWinHello, QString::fromUtf8("🔑") },
		{ &st::menuIconUsername, QString::fromUtf8("👤") },
		{ &st::menuIconUnpin, QString::fromUtf8("📌") },
		{ &st::menuIconTouchID, QString::fromUtf8("🔑") },
		{ &st::menuIconTopics, QString::fromUtf8("📑") },
		{ &st::menuIconTimer, QString::fromUtf8("⏳") },
		{ &st::menuIconTagSell, QString::fromUtf8("💰") },
		{ &st::menuIconTagRemove, QString::fromUtf8("❌") },
		{ &st::menuIconSystemPwd, QString::fromUtf8("🔓") },
		{ &st::menuIconStorage, QString::fromUtf8("📁") },
		{ &st::menuIconStealth, QString::fromUtf8("🫣") },
		{ &st::menuIconStats, QString::fromUtf8("📊") },
		{ &st::menuIconStartStream, QString::fromUtf8("🎙") },
		{ &st::menuIconSoundOn, QString::fromUtf8("🎵") },
		{ &st::menuIconSettings, QString::fromUtf8("⚙️") },
		{ &st::menuIconQrCode, QString::fromUtf8("📐") },
		{ &st::menuIconPhoto, QString::fromUtf8("🖼") },
		{ &st::menuIconNftWear, QString::fromUtf8("👑") },
		{ &st::menuIconNftTakeOff, QString::fromUtf8("👑") },
		{ &st::menuIconNewWindow, QString::fromUtf8("➕") },
		{ &st::menuIconMarkUnread, QString::fromUtf8("🔴") },
		{ &st::menuIconInviteSettings, QString::fromUtf8("🔗") },
		{ &st::menuIconInvite, QString::fromUtf8("➕") },
		{ &st::menuIconInfo, QString::fromUtf8("ℹ️") },
		{ &st::menuIconImportTheme, QString::fromUtf8("🖼") },
		{ &st::menuIconFaq, QString::fromUtf8("❔") },
		{ &st::menuIconExport, QString::fromUtf8("📤") },
		{ &st::menuIconExperimental, QString::fromUtf8("🔧") },
		{ &st::menuIconEarn, QString::fromUtf8("💰") },
		{ &st::menuIconDownload, QString::fromUtf8("🔽") },
		{ &st::menuIconDockBounce, QString::fromUtf8("🎈") },
		{ &st::menuIconCreatePoll, QString::fromUtf8("📊") },
		{ &st::menuIconCraft, QString::fromUtf8("🔧") },
		{ &st::menuIconCancel, QString::fromUtf8("❌") },
		{ &st::menuIconCallsReceive, QString::fromUtf8("📞") },
		{ &st::menuIconBlockSettings, QString::fromUtf8("🚫") },
		{ &st::menuIconArchive, QString::fromUtf8("📁") },
		{ &st::menuIconAppleWatch, QString::fromUtf8("🔘") },
		{ &st::menuIconAddress, QString::fromUtf8("📍") },
		{ &st::menuIconAddAccount, QString::fromUtf8("➕") },
		{ &st::menuIcon2SV, QString::fromUtf8("🔑") },
		{ &st::menuIconAbove, QString::fromUtf8("🔼") },
		{ &st::menuIconAdd, QString::fromUtf8("➕") },
		{ &st::menuIconAddToFolder, QString::fromUtf8("📁") },
		{ &st::menuIconAdmin, QString::fromUtf8("👮") },
		{ &st::menuIconAntispam, QString::fromUtf8("🛡") },
		{ &st::menuIconArchiveOpen, QString::fromUtf8("📁") },
		{ &st::menuIconArticle, QString::fromUtf8("📄") },
		{ &st::menuIconAsMessages, QString::fromUtf8("💬") },
		{ &st::menuIconAsTopics, QString::fromUtf8("📑") },
		{ &st::menuIconAuctionDrop, QString::fromUtf8("📈") },
		{ &st::menuIconBelow, QString::fromUtf8("🔽") },
		{ &st::menuIconBlockAttention, QString::fromUtf8("🚫") },
		{ &st::menuIconBoosts, QString::fromUtf8("💎") },
		{ &st::menuIconBot, QString::fromUtf8("🤖") },
		{ &st::menuIconCancelAttention, QString::fromUtf8("❌") },
		{ &st::menuIconCancelFee, QString::fromUtf8("❌") },
		{ &st::menuIconCaptionHide, QString::fromUtf8("🫣") },
		{ &st::menuIconCaptionShow, QString::fromUtf8("👁") },
		{ &st::menuIconChangeOrder, QString::fromUtf8("🔽") },
		{ &st::menuIconChatDiscuss, QString::fromUtf8("💬") },
		{ &st::menuIconChats, QString::fromUtf8("💬") },
		{ &st::menuIconClear, QString::fromUtf8("🗑") },
		{ &st::menuIconCollapse, QString::fromUtf8("🔼") },
		{ &st::menuIconCommunity, QString::fromUtf8("👥") },
		{ &st::menuIconCommunityRemoveAttention, QString::fromUtf8("⛔️") },
		{ &st::menuIconCopyright, QString::fromUtf8("📄") },
		{ &st::menuIconCraftChance, QString::fromUtf8("🎲") },
		{ &st::menuIconCraftTraits, QString::fromUtf8("🔧") },
		{ &st::menuIconCreateTodoList, QString::fromUtf8("☑️") },
		{ &st::menuIconCustomize, QString::fromUtf8("🎈") },
		{ &st::menuIconDisableAttention, QString::fromUtf8("⛔️") },
		{ &st::menuIconDownloadOff, QString::fromUtf8("⛔️") },
		{ &st::menuIconDraw, QString::fromUtf8("✏️") },
		{ &st::menuIconDrugs, QString::fromUtf8("⚠️") },
		{ &st::menuIconEnlarge, QString::fromUtf8("➕") },
		{ &st::menuIconExpand, QString::fromUtf8("🔽") },
		{ &st::menuIconExportTheme, QString::fromUtf8("📤") },
		{ &st::menuIconFactcheck, QString::fromUtf8("✅") },
		{ &st::menuIconFake, QString::fromUtf8("⚠️") },
		{ &st::menuIconFave, QString::fromUtf8("⭐️") },
		{ &st::menuIconFile, QString::fromUtf8("📄") },
		{ &st::menuIconForward, QString::fromUtf8("➡️") },
		{ &st::menuIconFromMainMenu, QString::fromUtf8("⬅️") },
		{ &st::menuIconGif, QString::fromUtf8("🎞") },
		{ &st::menuIconGroupLog, QString::fromUtf8("📋") },
		{ &st::menuIconGroupsHide, QString::fromUtf8("🫣") },
		{ &st::menuIconHourglass, QString::fromUtf8("⏳") },
		{ &st::menuIconKeysLegend, QString::fromUtf8("🔑") },
		{ &st::menuIconLeave, QString::fromUtf8("⛔️") },
		{ &st::menuIconLike, QString::fromUtf8("💟") },
		{ &st::menuIconLinks, QString::fromUtf8("🔗") },
		{ &st::menuIconMarkRead, QString::fromUtf8("✅") },
		{ &st::menuIconMute, QString::fromUtf8("🔕") },
		{ &st::menuIconMuteFor, QString::fromUtf8("🔕") },
		{ &st::menuIconMuteForAny, QString::fromUtf8("🔕") },
		{ &st::menuIconOrderDate, QString::fromUtf8("🗓") },
		{ &st::menuIconOrderNumber, QString::fromUtf8("#️⃣") },
		{ &st::menuIconOrderPrice, QString::fromUtf8("💰") },
		{ &st::menuIconPayment, QString::fromUtf8("💰") },
		{ &st::menuIconPendingRequests, QString::fromUtf8("⏳") },
		{ &st::menuIconPersonal, QString::fromUtf8("👤") },
		{ &st::menuIconPhotoSet, QString::fromUtf8("🖼") },
		{ &st::menuIconPhotoSuggest, QString::fromUtf8("🖼") },
		{ &st::menuIconPorn, QString::fromUtf8("⚠️") },
		{ &st::menuIconPromote, QString::fromUtf8("📈") },
		{ &st::menuIconQualityHigh, QString::fromUtf8("📹") },
		{ &st::menuIconRatingGifts, QString::fromUtf8("🎁") },
		{ &st::menuIconRatingRefund, QString::fromUtf8("↩️") },
		{ &st::menuIconRatingUsers, QString::fromUtf8("👥") },
		{ &st::menuIconReactions, QString::fromUtf8("💟") },
		{ &st::menuIconRemove, QString::fromUtf8("❌") },
		{ &st::menuIconRemoveAttention, QString::fromUtf8("❌") },
		{ &st::menuIconRemovedUsers, QString::fromUtf8("🚫") },
		{ &st::menuIconReorder, QString::fromUtf8("🔽") },
		{ &st::menuIconReply, QString::fromUtf8("↩️") },
		{ &st::menuIconReport, QString::fromUtf8("⚠️") },
		{ &st::menuIconReportAntiSpam, QString::fromUtf8("🛡") },
		{ &st::menuIconReportAttention, QString::fromUtf8("⚠️") },
		{ &st::menuIconReschedule, QString::fromUtf8("🗓") },
		{ &st::menuIconRestartBot, QString::fromUtf8("🤖") },
		{ &st::menuIconRestore, QString::fromUtf8("↩️") },
		{ &st::menuIconRestoreAttention, QString::fromUtf8("↩️") },
		{ &st::menuIconRetractVote, QString::fromUtf8("↩️") },
		{ &st::menuIconSavedMessages, QString::fromUtf8("⭐️") },
		{ &st::menuIconSaveImage, QString::fromUtf8("🔽") },
		{ &st::menuIconSchedule, QString::fromUtf8("🗓") },
		{ &st::menuIconSearch, QString::fromUtf8("👁") },
		{ &st::menuIconSelect, QString::fromUtf8("✔️") },
		{ &st::menuIconSend, QString::fromUtf8("➡️") },
		{ &st::menuIconShareOff, QString::fromUtf8("🚫") },
		{ &st::menuIconShareOn, QString::fromUtf8("📤") },
		{ &st::menuIconSharing, QString::fromUtf8("📤") },
		{ &st::menuIconShowAll, QString::fromUtf8("👁") },
		{ &st::menuIconShrink, QString::fromUtf8("🔼") },
		{ &st::menuIconSigned, QString::fromUtf8("✏️") },
		{ &st::menuIconSilent, QString::fromUtf8("🔕") },
		{ &st::menuIconSoundAdd, QString::fromUtf8("🎵") },
		{ &st::menuIconSoundOff, QString::fromUtf8("🔕") },
		{ &st::menuIconSoundSelect, QString::fromUtf8("🎵") },
		{ &st::menuIconSpoiler, QString::fromUtf8("🫣") },
		{ &st::menuIconStar, QString::fromUtf8("⭐️") },
		{ &st::menuIconStarRefLink, QString::fromUtf8("🔗") },
		{ &st::menuIconStarRefShare, QString::fromUtf8("📤") },
		{ &st::menuIconStarsCarryover, QString::fromUtf8("⭐️") },
		{ &st::menuIconStarsRefund, QString::fromUtf8("↩️") },
		{ &st::menuIconStartStreamWith, QString::fromUtf8("🎙") },
		{ &st::menuIconStealthLocked, QString::fromUtf8("🫣") },
		{ &st::menuIconStickerAdd, QString::fromUtf8("🙂") },
		{ &st::menuIconStickerCreate, QString::fromUtf8("🙂") },
		{ &st::menuIconStoriesArchive, QString::fromUtf8("📁") },
		{ &st::menuIconStoriesArchiveSection, QString::fromUtf8("📁") },
		{ &st::menuIconStoriesSave, QString::fromUtf8("🔽") },
		{ &st::menuIconStoriesSavedSection, QString::fromUtf8("⭐️") },
		{ &st::menuIconTableSubmenuDelete, QString::fromUtf8("🗑") },
		{ &st::menuIconTagAdd, QString::fromUtf8("➕") },
		{ &st::menuIconTagEdit, QString::fromUtf8("✏️") },
		{ &st::menuIconTagFilter, QString::fromUtf8("📑") },
		{ &st::menuIconTagRemoveAttention, QString::fromUtf8("❌") },
		{ &st::menuIconTagRename, QString::fromUtf8("✏️") },
		{ &st::menuIconToMainMenu, QString::fromUtf8("➡️") },
		{ &st::menuIconTradable, QString::fromUtf8("📈") },
		{ &st::menuIconTransparent, QString::fromUtf8("🔘") },
		{ &st::menuIconUnarchive, QString::fromUtf8("📁") },
		{ &st::menuIconUnblock, QString::fromUtf8("✅") },
		{ &st::menuIconUnfave, QString::fromUtf8("❌") },
		{ &st::menuIconUnique, QString::fromUtf8("💎") },
		{ &st::menuIconUniqueProfile, QString::fromUtf8("💎") },
		{ &st::menuIconUserHide, QString::fromUtf8("🫣") },
		{ &st::menuIconUserHideDisabled, QString::fromUtf8("👁") },
		{ &st::menuIconUserShow, QString::fromUtf8("👁") },
		{ &st::menuIconVideoChat, QString::fromUtf8("📹") },
		{ &st::menuIconViewReplies, QString::fromUtf8("💬") },
		{ &st::menuIconViolence, QString::fromUtf8("⚠️") },
		{ &st::menuIconWelcomeMessage, QString::fromUtf8("👋") },
		{ &st::menuIconWhenOnline, QString::fromUtf8("🔵") },
		{ &st::menuIconZoomIn, QString::fromUtf8("➕") },
		{ &st::menuIconZoomOut, QString::fromUtf8("🔽") },
	};
	return map;
}

[[nodiscard]] Window::SessionController *ActiveController() {
	const auto window = Core::App().activePrimaryWindow();
	return window ? window->sessionController() : nullptr;
}

} // namespace

const style::icon &OwnRowIcon() {
	static const style::icon result = st::menuIconPalette;
	return result;
}

bool HasRowIcon(const void *icon) {
	// Ждём набор только первые секунды после запуска: нет сети, значит рисуем старые.
	static const auto started = crl::now();
	return RowIconMap().contains(icon) && (crl::now() - started < 8000);
}

std::unique_ptr<Ui::Text::CustomEmoji> RowIconEmoji(
		const void *icon,
		Fn<void()> repaint,
		int size) {
	const auto &map = RowIconMap();
	const auto i = map.find(icon);
	const auto controller = ActiveController();
	if (i == map.end() || !controller) {
		return nullptr;
	}
	auto &pack = PackIcons::Instance();
	pack.ensure(&controller->session());
	return pack.makeAlt(i->second, std::move(repaint), size);
}

std::unique_ptr<Ui::Text::CustomEmoji> AltIconEmoji(
		const QString &alt,
		Fn<void()> repaint,
		int size) {
	const auto controller = ActiveController();
	if (!controller) {
		return nullptr;
	}
	auto &pack = PackIcons::Instance();
	pack.ensure(&controller->session());
	return pack.makeAlt(alt, std::move(repaint), size);
}

rpl::producer<> RowIconsUpdated() {
	return PackIcons::Instance().updated();
}

} // namespace Mokagram::Settings
