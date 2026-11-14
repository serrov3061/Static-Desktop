/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Data {
class SavedStarGiftId;
} // namespace Data

// StaticGram profile gift tiles (Android GiftTileLayouts, gramsrv
// docs/sggifts.md): a tile covers w columns (1..3) by h rows (1..2) of the
// profile Gifts grid, 1x1 is the default and is not stored. The sizes are
// cached per peer; sggifts.getTileSizes / saveTileSizes, falling back to
// the wide-only getTileLayout / saveTileLayout on an older server.
namespace StaticGram::GiftTiles {

inline constexpr auto kMaxW = 3;
inline constexpr auto kMaxH = 2;
inline constexpr auto kMaxTiles = 100; // saveTileSizes limit.

struct Size {
	int w = 1;
	int h = 1;

	[[nodiscard]] bool single() const {
		return (w == 1) && (h == 1);
	}
	friend inline bool operator==(const Size &, const Size &) = default;
};

[[nodiscard]] Size Normalized(int w, int h);

// The tile key of a gift: msg_id for gifts of a user, saved_id for a chat.
using Sizes = base::flat_map<uint64, Size>;
[[nodiscard]] uint64 Key(const Data::SavedStarGiftId &id);

// The non-1x1 sizes known for the peer (empty until loaded).
[[nodiscard]] Sizes Cached(not_null<PeerData*> peer);

// Fetches the sizes, `done` is called with the fresh cache on success.
void Load(not_null<PeerData*> peer, Fn<void(const Sizes&)> done);

// Replaces the sizes on the server with every non-1x1 entry (hidden or
// not loaded gifts included); `done` gets an empty error on success.
void Save(
	not_null<PeerData*> peer,
	const Sizes &sizes,
	Fn<void(QString error)> done);

} // namespace StaticGram::GiftTiles
