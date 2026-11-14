/*
This file is part of StaticGram Desktop, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "staticgram/sg_tag_colors.h"

#include "apiwrap.h"
#include "data/data_channel.h"
#include "data/data_chat_participant_status.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/view/history_view_element.h"
#include "history/view/history_view_message.h"
#include "main/main_session.h"
#include "ui/chat/chat_style.h"
#include "styles/style_chat.h"
#include "styles/style_widgets.h"

#include <QtCore/QDateTime>

namespace StaticGram::TagColors {
namespace {

constexpr auto kReloadInterval = crl::time(30000);

struct Chat {
	base::flat_map<UserId, QRgb> colors;
	crl::time lastLoad = 0;
	rpl::event_stream<> changes;
};

[[nodiscard]] base::flat_map<
	std::pair<Main::Session*, ChannelId>,
	std::unique_ptr<Chat>> &Chats() {
	static auto result = base::flat_map<
		std::pair<Main::Session*, ChannelId>,
		std::unique_ptr<Chat>>();
	return result;
}

[[nodiscard]] Chat &Lookup(not_null<ChannelData*> channel) {
	auto &map = Chats();
	const auto key = std::make_pair(&channel->session(), peerToChannel(channel->id));
	auto i = map.find(key);
	if (i == map.end()) {
		i = map.emplace(key, std::make_unique<Chat>()).first;
	}
	return *i->second;
}

[[nodiscard]] ChannelData *Megagroup(not_null<PeerData*> peer) {
	const auto channel = peer->asMegagroup();
	return (channel && !channel->isForbidden()) ? channel : nullptr;
}

void Repaint(not_null<ChannelData*> channel) {
	const auto history = channel->owner().historyLoaded(channel);
	if (!history) {
		return;
	}
	for (const auto &block : history->blocks) {
		for (const auto &view : block->messages) {
			channel->owner().requestViewRepaint(view.get());
		}
	}
}

void Notify(not_null<ChannelData*> channel) {
	Lookup(channel).changes.fire({});
	Repaint(channel);
}

// Telegram sets the "other" right for an admin without any other right.
[[nodiscard]] bool TitleOnly(
		not_null<PeerData*> peer,
		not_null<UserData*> user) {
	const auto channel = peer->asMegagroup();
	const auto info = channel ? channel->mgInfo.get() : nullptr;
	if (!info) {
		return false;
	}
	const auto i = info->lastAdmins.find(user);
	if (i == info->lastAdmins.end()) {
		return false;
	}
	const auto mask = ~(ChatAdminRight::Other | ChatAdminRight::Anonymous);
	return !(i->second.rights.flags & mask);
}

} // namespace

const std::vector<QColor> &Palette() {
	static const auto result = std::vector<QColor>{
		QColor(0xE5, 0x39, 0x35), // red
		QColor(0xF5, 0x7C, 0x00), // orange
		QColor(0xD4, 0xA2, 0x0B), // amber
		QColor(0x40, 0xA9, 0x20), // green
		QColor(0x00, 0xA3, 0xA3), // teal
		QColor(0x33, 0x90, 0xEC), // blue
		QColor(0x8E, 0x59, 0xD9), // purple
		QColor(0xE0, 0x56, 0x9A), // pink
		QColor(0x8D, 0x99, 0xA6), // grey
	};
	return result;
}

QColor DefaultColor(HistoryView::BadgeRole role, bool titleOnly) {
	using HistoryView::BadgeRole;
	switch (role) {
	case BadgeRole::Creator: return st::rankOwnerFg->c;
	case BadgeRole::Admin:
		return titleOnly ? st::rankAdminFg->c : Palette().front();
	case BadgeRole::User: break;
	}
	return st::rankUserFg->c;
}

QColor Picked(not_null<PeerData*> peer, not_null<UserData*> user) {
	const auto channel = Megagroup(peer);
	if (!channel) {
		return QColor();
	}
	const auto &colors = Lookup(channel).colors;
	const auto i = colors.find(peerToUser(user->id));
	return (i != colors.end()) ? QColor::fromRgb(i->second) : QColor();
}

QColor Resolve(
		not_null<PeerData*> peer,
		not_null<UserData*> user,
		HistoryView::BadgeRole role) {
	if (const auto picked = Picked(peer, user); picked.isValid()) {
		return picked;
	}
	return DefaultColor(
		role,
		(role == HistoryView::BadgeRole::Admin) && TitleOnly(peer, user));
}

void Request(not_null<PeerData*> peer, bool force) {
	const auto channel = Megagroup(peer);
	if (!channel) {
		return;
	}
	auto &chat = Lookup(channel);
	const auto now = crl::now();
	if (!force && chat.lastLoad && now - chat.lastLoad < kReloadInterval) {
		return;
	}
	chat.lastLoad = now ? now : 1;
	const auto weak = base::make_weak(channel);
	channel->session().api().request(MTPsgtags_GetTagColors(
		channel->input()
	)).done([=](const MTPsgtags_TagColors &result) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		auto colors = base::flat_map<UserId, QRgb>();
		result.match([&](const MTPDsgtags_tagColors &data) {
			for (const auto &item : data.vcolors().v) {
				item.match([&](const MTPDsgtags_tagColor &c) {
					colors.emplace(
						UserId(c.vuser_id().v),
						QRgb(0xFF000000 | (uint32(c.vcolor().v) & 0xFFFFFF)));
				});
			}
		});
		auto &chat = Lookup(strong);
		if (chat.colors != colors) {
			chat.colors = std::move(colors);
			Notify(strong);
		}
	}).send();
}

void Set(
		not_null<PeerData*> peer,
		not_null<UserData*> user,
		QColor color,
		Fn<void(bool ok)> done) {
	const auto channel = Megagroup(peer);
	if (!channel) {
		if (done) {
			done(false);
		}
		return;
	}
	using Flag = MTPsgtags_setTagColor::Flag;
	const auto weak = base::make_weak(channel);
	const auto userId = peerToUser(user->id);
	const auto valid = color.isValid();
	const auto rgb = valid ? (color.rgb() & 0xFFFFFF) : 0;
	channel->session().api().request(MTPsgtags_SetTagColor(
		MTP_flags(valid ? Flag::f_color : Flag(0)),
		channel->input(),
		user->input(),
		MTP_int(int(rgb))
	)).done([=] {
		if (const auto strong = weak.get()) {
			auto &colors = Lookup(strong).colors;
			if (valid) {
				colors[userId] = QRgb(0xFF000000 | rgb);
			} else {
				colors.erase(userId);
			}
			Notify(strong);
		}
		if (done) {
			done(true);
		}
	}).fail([=] {
		if (done) {
			done(false);
		}
	}).send();
}

rpl::producer<> Changes(not_null<PeerData*> peer) {
	const auto channel = Megagroup(peer);
	if (!channel) {
		return rpl::never<>();
	}
	return Lookup(channel).changes.events();
}

} // namespace StaticGram::TagColors
