// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mokagram-id: 93ab7964
// Mokagram: SDK плагинов на стороне клиента (мост к Qt) и входы хуков обновлений.
// StaticGram: the mokaGram plugin system is removed; inert hook only.
#pragma once

class PeerData;

namespace Mokagram::Plugins {

// Does nothing.
void EmitChatOpened(PeerData *peer);

} // namespace Mokagram::Plugins
