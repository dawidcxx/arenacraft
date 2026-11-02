#pragma once

#include "ObjectGuid.h"

class Player;

// VIP characters: a per-character flag set by administrators (`.vip add|del|get`), persisted in
// `character_vip` and cached in memory. VIPs get perks such as transmog without the quality and
// armor-type rules.
namespace arenacraft::vip
{
// Loads every VIP character into the cache. Runs once at world startup.
void Load();

[[nodiscard]] bool IsVip(ObjectGuid guid);
[[nodiscard]] bool IsVip(Player const* player);

// Updates the cache and the database. Returns false when the character already had that status.
bool Set(ObjectGuid guid, bool vip);

// Registers the startup loader and the `.vip` command.
void AddVipScripts();
} // namespace arenacraft::vip
