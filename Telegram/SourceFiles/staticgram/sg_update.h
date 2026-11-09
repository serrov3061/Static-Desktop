/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace StaticGram {

// A mandatory release (can_not_skip / min_version in our update feed, see
// Core::ForcedUpdateValue()) shows a box that can't be closed until the
// update is installed, like the Android client: it follows the download
// and offers "Restart" once the package is ready, or a download link when
// the build can't update itself (an AppImage).
void SetupForcedUpdate(rpl::lifetime &lifetime);

} // namespace StaticGram
