/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

class PeerData;
class UserData;
class ChannelData;

namespace HistoryView {
enum class BadgeRole : uchar;
} // namespace HistoryView

// StaticGram member tag colors (Android MemberTagColors): owner purple,
// admin with rights red, title-only admin (a bought tag) green, any other
// label gray. A color picked in the tag box is stored on the server
// (sgtags.*) and overrides the default for anyone.
namespace StaticGram::TagColors {

// Picker palette, same as Android MemberTagColors.PALETTE.
[[nodiscard]] const std::vector<QColor> &Palette();

// Color the tag of the user is drawn with in this group.
[[nodiscard]] QColor Resolve(
	not_null<PeerData*> peer,
	not_null<UserData*> user,
	HistoryView::BadgeRole role);
[[nodiscard]] QColor DefaultColor(HistoryView::BadgeRole role, bool titleOnly);

// Picked color or invalid QColor.
[[nodiscard]] QColor Picked(
	not_null<PeerData*> peer,
	not_null<UserData*> user);

// Fetches colors of a megagroup (throttled unless forced).
void Request(not_null<PeerData*> peer, bool force = false);

// Sends sgtags.setTagColor (invalid color clears it).
void Set(
	not_null<PeerData*> peer,
	not_null<UserData*> user,
	QColor color,
	Fn<void(bool ok)> done);

// Fires when colors of the group were (re)loaded or changed.
[[nodiscard]] rpl::producer<> Changes(not_null<PeerData*> peer);

} // namespace StaticGram::TagColors
