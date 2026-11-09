/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "staticgram/sg_avatar_frames.h"

namespace Main {
class Session;
} // namespace Main

class UserData;

namespace StaticGram {

// StaticGram badge settings from help.getAppConfig (sg_badges_*), with the
// same fallbacks as Android BadgesConfig.
struct BadgesConfig {
	QString apiUrl;
	QString bot; // Without @.
	QString shopUrl;
	QString priceSupporter;
	QString priceCustom;
	QString priceUpgrade;
	DocumentId defaultDocumentId = 0;
	DocumentId developerDocumentId = 0;
};
[[nodiscard]] BadgesConfig ReadBadgesConfig(
	not_null<Main::Session*> session);

// Uses the app config list URL and syncs the badges after each app config
// refresh (Android: sync after the config arrives).
void TrackBadgesSession(not_null<Main::Session*> session);

// Inline query to the badge bot (sg_badges_bot, @StaticBadgesBot), done gets
// results[0].send_message.message or an empty string on failure.
void SendBadgesBotRequest(
	not_null<Main::Session*> session,
	const QString &query,
	Fn<void(QString)> done);

// The badge bot user (resolved by username when not known yet), nullptr when
// there is none.
void WithBadgesBot(
	not_null<Main::Session*> session,
	Fn<void(UserData*)> done);

// "banner <documentId> <accessHash>" / "banner none" -> "ok" (then a local
// update and a sync). The document is uploaded with UploadBanner.
void ApplyBanner(
	not_null<Main::Session*> session,
	DocumentId documentId,
	uint64 accessHash,
	bool video,
	Fn<void(QString)> done);

// "badge <documentId> [text]" -> "ok" (then a local update and a sync).
void ApplyBadge(
	not_null<Main::Session*> session,
	DocumentId documentId,
	const QString &text,
	Fn<void(QString)> done);

// "frame <style> key=value ..." / "frame none" -> "ok".
void ApplyAvatarFrame(
	not_null<Main::Session*> session,
	std::optional<AvatarFrame> frame,
	Fn<void(QString)> done);

// "frames" -> the styles this user may pick ("error: ..." = none).
void RefreshAllowedFrames(
	not_null<Main::Session*> session,
	Fn<void()> done = nullptr);
[[nodiscard]] bool AllowedFramesKnown(not_null<Main::Session*> session);

// Any badge (or an admin frame grant) includes every style; the bot's
// "frames" answer decides for the current user once it is known.
[[nodiscard]] bool CanUseFrames(not_null<Main::Session*> session);
[[nodiscard]] bool CanUseFrame(
	not_null<Main::Session*> session,
	const QString &style);

} // namespace StaticGram
