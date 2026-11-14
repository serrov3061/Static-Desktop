/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/object_ptr.h"

class UserData;

namespace Ui {
class RpWidget;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace StaticGram {

// exteraGram «Now playing» card (Android NowPlayingCard): the profile
// music as a rounded plate under the action buttons — the cover on the
// left, title and artist, the background in the profile colors with the
// profile pattern. A click plays the track, «next» goes through the
// user's saved music. Hidden while the user has no profile music.
[[nodiscard]] object_ptr<Ui::RpWidget> CreateProfileMusicPlate(
	not_null<Ui::RpWidget*> parent,
	not_null<Window::SessionController*> controller,
	not_null<UserData*> user);

// Whether the plate exists for this user (shown or not).
[[nodiscard]] rpl::producer<bool> ProfileMusicPlateShownValue(
	not_null<UserData*> user);

} // namespace StaticGram
