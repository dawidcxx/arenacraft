#pragma once

#include <cstdint>
#include <optional>

class ArenaTeam;
class Player;

namespace arenacraft::soloq
{
struct SoloqTeamInfo
{
  uint32_t rating;
  uint32_t mmr;
};

// The solo-queue "team" is a real 5v5 ArenaTeam captained by the player, so it
// shows up in the client's PvP pane and persists in the characters DB.
ArenaTeam* FindSoloqTeam(Player* player);

// Creates the player's 5v5 team at the starting rating/MMR. Fails when the
// player is already in a 5v5 team.
bool CreateSoloqTeam(Player* player);

// Disbands the player's solo-queue team (resets rating/MMR).
bool DeleteSoloqTeam(Player* player);

std::optional<SoloqTeamInfo> GetSoloqTeamInfo(Player* player);
} // namespace arenacraft::soloq
