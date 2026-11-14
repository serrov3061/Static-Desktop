/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_gift_tiles.h"

#include "apiwrap.h"
#include "data/data_peer.h"
#include "data/data_star_gift.h"
#include "main/main_session.h"

namespace StaticGram::GiftTiles {
namespace {

[[nodiscard]] base::flat_map<not_null<PeerData*>, Sizes> &Cache() {
	static auto result = base::flat_map<not_null<PeerData*>, Sizes>();
	return result;
}

void Store(not_null<PeerData*> peer, Sizes sizes) {
	auto &cache = Cache();
	if (!cache.contains(peer)) {
		// Peers live as long as their session.
		peer->session().lifetime().add([=] {
			Cache().remove(peer);
		});
	}
	cache[peer] = std::move(sizes);
}

[[nodiscard]] bool MethodMissing(const MTP::Error &error) {
	const auto &type = error.type();
	return type.contains(u"METHOD_INVALID"_q)
		|| type.contains(u"CONSTRUCTOR_INVALID"_q)
		|| (error.code() == 501);
}

[[nodiscard]] MTPInputSavedStarGift Input(
		not_null<PeerData*> peer,
		uint64 key) {
	return peer->isUser()
		? MTP_inputSavedStarGiftUser(MTP_int(int(key)))
		: MTP_inputSavedStarGiftChat(peer->input(), MTP_long(key));
}

void LoadLegacy(not_null<PeerData*> peer, Fn<void(const Sizes&)> done) {
	peer->session().api().request(MTPsggifts_GetTileLayout(
		peer->input()
	)).done([=](const MTPsggifts_TileLayout &result) {
		auto sizes = Sizes();
		for (const auto &tile : result.data().vwide().v) {
			const auto &data = tile.data();
			const auto key = data.vmsg_id()
				? uint64(data.vmsg_id()->v)
				: data.vsaved_id()
				? uint64(data.vsaved_id()->v)
				: uint64();
			if (key) {
				sizes[key] = Size{ 2, 1 };
			}
		}
		Store(peer, sizes);
		if (done) {
			done(sizes);
		}
	}).send();
}

void SaveLegacy(
		not_null<PeerData*> peer,
		const Sizes &sizes,
		Fn<void(QString error)> done) {
	auto wide = QVector<MTPInputSavedStarGift>();
	auto saved = Sizes();
	for (const auto &[key, size] : sizes) {
		if (size.w >= 2) {
			wide.push_back(Input(peer, key));
			saved[key] = Size{ 2, 1 };
		}
	}
	peer->session().api().request(MTPsggifts_SaveTileLayout(
		peer->input(),
		MTP_vector<MTPInputSavedStarGift>(std::move(wide))
	)).done([=] {
		Store(peer, saved);
		if (done) {
			done(QString());
		}
	}).fail([=](const MTP::Error &error) {
		if (done) {
			done(error.type());
		}
	}).send();
}

} // namespace

Size Normalized(int w, int h) {
	return Size{ std::clamp(w, 1, kMaxW), std::clamp(h, 1, kMaxH) };
}

uint64 Key(const Data::SavedStarGiftId &id) {
	return id.isUser()
		? uint64(id.userMessageId().bare)
		: id.chatSavedId();
}

Sizes Cached(not_null<PeerData*> peer) {
	const auto &cache = Cache();
	const auto i = cache.find(peer);
	return (i != end(cache)) ? i->second : Sizes();
}

void Load(not_null<PeerData*> peer, Fn<void(const Sizes&)> done) {
	peer->session().api().request(MTPsggifts_GetTileSizes(
		peer->input()
	)).done([=](const MTPsggifts_TileSizes &result) {
		auto sizes = Sizes();
		for (const auto &tile : result.data().vtiles().v) {
			const auto &data = tile.data();
			const auto key = data.vmsg_id()
				? uint64(data.vmsg_id()->v)
				: data.vsaved_id()
				? uint64(data.vsaved_id()->v)
				: uint64();
			const auto size = Normalized(data.vw().v, data.vh().v);
			if (key && !size.single()) {
				sizes[key] = size;
			}
		}
		Store(peer, sizes);
		if (done) {
			done(sizes);
		}
	}).fail([=](const MTP::Error &error) {
		if (MethodMissing(error)) {
			LoadLegacy(peer, done);
		}
	}).send();
}

void Save(
		not_null<PeerData*> peer,
		const Sizes &sizes,
		Fn<void(QString error)> done) {
	auto tiles = QVector<MTPsggifts_InputTileSize>();
	auto copy = Sizes();
	for (const auto &[key, size] : sizes) {
		if (!key || size.single()) {
			continue;
		}
		copy[key] = size;
		tiles.push_back(MTP_sggifts_inputTileSize(
			Input(peer, key),
			MTP_int(size.w),
			MTP_int(size.h)));
	}
	if (tiles.size() > kMaxTiles) {
		if (done) {
			done(u"LIMIT_INVALID"_q);
		}
		return;
	}
	peer->session().api().request(MTPsggifts_SaveTileSizes(
		peer->input(),
		MTP_vector<MTPsggifts_InputTileSize>(std::move(tiles))
	)).done([=] {
		Store(peer, copy);
		if (done) {
			done(QString());
		}
	}).fail([=](const MTP::Error &error) {
		if (MethodMissing(error)) {
			SaveLegacy(peer, copy, done);
		} else if (done) {
			done(error.type());
		}
	}).send();
}

} // namespace StaticGram::GiftTiles
