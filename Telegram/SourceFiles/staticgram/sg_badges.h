/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "staticgram/sg_avatar_frames.h"

class PeerData;

namespace StaticGram {

// exteraStatic badges and avatar frames (Android BadgesController /
// ApiController, iOS StaticGramBadges.swift): the public profile list
// GET https://badges.staticgram.top/api/v1/profiles
// [{"id", "type": "USER"|"CHAT", "status", "badge": {"documentId", "text"},
//   "canChangeBadge", "deleted", "frame", "frame_params": {...}}]
// fetched whole (with ETag) at most every minute while the UI asks for it
// and kept in tdata, so it is ready right after launch.

struct UserBadge {
	DocumentId documentId = 0;
	QString text;
	bool canChange = false; // A custom (or developer) badge.
	bool developer = false;
	int number = 0; // The supporter badge number, 0 when there is none.

	explicit operator bool() const {
		return documentId != 0;
	}
};

// The custom emoji shown as the peer's badge next to its name, or 0.
[[nodiscard]] DocumentId BadgeDocumentId(not_null<PeerData*> peer);

// The full badge of a user (bare id), empty when there is none.
[[nodiscard]] UserBadge BadgeOfUser(uint64 userId);

// The avatar frame of a user (chats and channels have none), or nullptr.
[[nodiscard]] const AvatarFrame *PeerAvatarFrame(not_null<PeerData*> peer);
[[nodiscard]] const AvatarFrame *UserAvatarFrame(uint64 userId);

// The profile banner of a user (picture or short video behind the profile
// header, only for a live badge), empty when there is none. The file is
// <list url base>banners/<id>.
struct ProfileBanner {
	QString id; // The document id, a decimal string.
	bool video = false;

	explicit operator bool() const {
		return !id.isEmpty();
	}
	friend inline bool operator==(
		const ProfileBanner &a,
		const ProfileBanner &b) {
		return (a.id == b.id) && (a.video == b.video);
	}
	friend inline bool operator!=(
		const ProfileBanner &a,
		const ProfileBanner &b) {
		return !(a == b);
	}
};
[[nodiscard]] ProfileBanner BannerOfUser(uint64 userId);
// The base of the banner files, ends with a slash.
[[nodiscard]] QString BadgesApiBase();

// Fires when the list changed.
[[nodiscard]] rpl::producer<> BadgesUpdated();
void NotifyBadgesChanged();

// The list URL base from app config (sg_badges_api_url), if any.
void SetBadgesApiUrl(const QString &url);

// Syncs now, ignoring the 1 minute throttle (but not more often than every
// 15 seconds), done is called when finished or skipped.
void SyncBadgesNow(Fn<void()> done = nullptr);

// Syncs when a profile opens, if the last sync is older than 30 seconds.
void SyncBadgesOnProfileOpen();

// After the badge bot answered "ok": shows the change before the sync.
void UpdateLocalBadge(uint64 userId, DocumentId documentId, QString text);
void UpdateLocalFrame(uint64 userId, std::optional<AvatarFrame> frame);
void UpdateLocalBanner(uint64 userId, ProfileBanner banner);

// After a badge transfer between two own accounts: the badge (with its
// number) moves before the sync.
void MoveLocalBadge(uint64 fromUserId, uint64 toUserId);

// Whether frames may animate now (power saving, disabled animations).
[[nodiscard]] bool AvatarFramesAnimated();
[[nodiscard]] bool AvatarFramesLight();

// Paints the peer's avatar frame (if any) around the photo, unless the
// peer has active stories (the story ring wins).
void PaintPeerAvatarFrame(
	QPainter &p,
	PeerData *peer,
	QRectF photo,
	QRectF allowed,
	double opacity = 1.);

} // namespace StaticGram
