#pragma once

#include "Types.hpp"

class Player;

namespace arenacraft::soloq
{
// Registers the player in the real 5v5 battleground queue so the client shows
// the "in queue" eye and re-requests it on login/map change. The queue is
// otherwise inert: SoloqBattlegroundScript suppresses the core's 5v5 rated
// matchmaking, so no arena is ever created from it.
bool EnterArenaQueue(Player* player);

// Removes the player from that 5v5 queue and clears the client badge. Safe to
// call when the player is not queued.
void LeaveArenaQueue(Player* player);

// True while the player is registered in the 5v5 queue (ours or otherwise).
bool InArenaQueue(Player* player);

// Creates a rated 5v5 arena instance for the six matched players and invites
// each team, which pops the client's "Enter Battle" dialog. Returns false if
// the match can no longer be turned into an arena (players no longer queued).
bool CreateArenaForMatch(Match const& match);
} // namespace arenacraft::soloq
