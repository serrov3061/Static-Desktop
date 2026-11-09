/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Main {
class Session;
} // namespace Main

namespace StaticGram {

// Profile banners (Android ProfileBanners / ProfileBannerUpload): a badge
// holder's picture or short video behind the profile header. The public
// profile list carries banner_id / banner_type, the file is
// <list url base>banners/<id>. Files are cached by document id in
// tdata/sg_banners (an id never changes its bytes).
inline constexpr auto kBannerMaxPhotoBytes = qint64(5) * 1024 * 1024;
inline constexpr auto kBannerMaxVideoBytes = qint64(10) * 1024 * 1024;
inline constexpr auto kBannerMaxVideoSeconds = 15;

// The decoded banner of a user: a picture, or the first frame of a video
// banner. Null when there is no banner or it is not downloaded yet (the
// download starts on the first call and fires BadgesUpdated when finished).
[[nodiscard]] QImage UserBannerImage(uint64 userId);

// Drops the decoded images (e.g. when a banner is removed).
void ClearBannerImages();

// Uploads the picked file as the own banner: a picture is re-encoded to a
// JPEG up to 1920 px wide and 5 MB, an MP4 video up to 15 s and 10 MB is
// sent as is (uploaded to the badge bot as a document, set with the badge
// bot "banner" query). done gets an empty string on success, otherwise
// "photo" / "video" (the file does not fit), "bot" or the failure reason.
void UploadBanner(
	not_null<Main::Session*> session,
	const QString &path,
	Fn<void(QString)> done);

// Removes the own banner ("banner none").
void RemoveBanner(
	not_null<Main::Session*> session,
	Fn<void(QString)> done);

} // namespace StaticGram
