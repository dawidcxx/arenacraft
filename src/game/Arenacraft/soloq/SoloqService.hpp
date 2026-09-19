#pragma once

#include "SoloqQueue.hpp"
#include "Types.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace arenacraft::soloq
{
// Owns the queue and the arenas waiting for a result. Player rating/MMR lives in
// the player's 5v5 ArenaTeam (see SoloqTeam.hpp), not here.
class SoloqService
{
public:
  static SoloqService& instance();

  bool join(PlayerId id, Classes classId, uint8_t specIndex, TeamId teamId, uint32_t rating, uint32_t mmr);
  bool leave(PlayerId id);

  std::vector<Match> tick(std::chrono::milliseconds elapsed);

  // Soloq arenas are keyed by battleground instance id until they end, so the
  // result can be fed back through resolveMatch.
  void                 registerMatch(uint32 bgInstanceId, Match const& match);
  std::optional<Match> takeMatch(uint32 bgInstanceId);
  void                 forgetMatch(uint32 bgInstanceId);

  struct PendingArena
  {
    uint32 bgInstanceId;
    Match  match;
  };

  [[nodiscard]] std::size_t                            queueSize() const;
  [[nodiscard]] bool                                   inQueue(PlayerId id) const;
  [[nodiscard]] std::vector<PlayerId>                  waitingPlayers() const;
  [[nodiscard]] std::vector<SoloqQueue::QueueSnapshot> snapshot() const;
  [[nodiscard]] std::vector<PendingArena>              pendingArenas() const;

  void               setEnforceTeamFaction(bool value);
  [[nodiscard]] bool enforceTeamFaction() const;

private:
  SoloqService() = default;

  SoloqQueue                        _queue;
  std::unordered_map<uint32, Match> _pendingMatches;
};
} // namespace arenacraft::soloq
