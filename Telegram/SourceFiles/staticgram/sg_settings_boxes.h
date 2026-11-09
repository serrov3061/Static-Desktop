/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

class PeerData;

namespace StaticGram {

// «exteraStatic badge» (Android BadgePreferencesActivity): preview, the
// avatar frame entry, the emoji and the caption of a custom badge, saved
// through the badge bot, or the upgrade / buy flow.
void ShowBadgeBox(not_null<Window::SessionController*> controller);

// «Avatar frame» (Android AvatarFramePreferencesActivity + AvatarFramePicker).
void ShowFrameBox(not_null<Window::SessionController*> controller);

// «Profile banner» (Android ProfileBannerPicker): a picture or a short video
// behind the profile header, for badge holders.
void ShowBannerBox(not_null<Window::SessionController*> controller);

// «Support StaticGram» (Android SupporterBottomSheet): prices from app
// config and the shop link.
void ShowSupporterBox(
	not_null<Window::SessionController*> controller,
	bool fromBadgeBox = false);

// Another user's (or chat's) badge clicked next to the name (Android
// SupporterPeerSheet); one's own badge opens ShowBadgeBox.
void ShowPeerBadgeBox(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer);

[[nodiscard]] QString AvatarFrameName(const QString &style);
[[nodiscard]] rpl::producer<QString> SelfAvatarFrameNameValue(
	not_null<Main::Session*> session);

} // namespace StaticGram
