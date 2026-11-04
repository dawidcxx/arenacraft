#pragma once

#include "Define.h"

#include <string>
#include <vector>

namespace arenacraft::fingerprint
{
// Remembers the addons an account's client reported at login (WorldSession::ReadAddonsInfo), so a fingerprint can
// be taken while the player is offline.
void RememberAddons(uint32 accountId, std::vector<std::string> const& enabledAddons);

// .fingerprint touch|compare|list|delete (game masters).
void AddFingerprintScripts();
} // namespace arenacraft::fingerprint
