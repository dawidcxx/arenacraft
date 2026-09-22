#pragma once

#include "CharacterCheck.hpp"
#include "SoloqQueue.hpp"
#include "Types.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace arenacraft::soloq
{
// Queued players per role. The queue is a single shared pool.
struct RoleCounts
{
  uint16_t melee  = 0;
  uint16_t caster = 0;
  uint16_t healer = 0;
};

// Pure: renders the shared queue line shown in the NPC gossip page.
std::string formatQueueStats(RoleCounts const& counts);

// Owns the queue and the arenas waiting for a result. Player rating/MMR lives in
// the player's 5v5 ArenaTeam (see SoloqTeam.hpp), not here.
class SoloqService
{
public:
  static SoloqService& instance();

  bool join(PlayerId id, Classes classId, uint8_t specIndex, TeamId teamId, uint32_t rating, uint32_t mmr);
  bool leave(PlayerId id);

  // Runs the character readiness check at most once per character per process:
  // a character that passes is remembered in _validatedCharacters. Returns the
  // reason the character cannot queue, or nullopt when it is ready. Always
  // reports ready while skipCharacterChecks is set (debug bypass).
  [[nodiscard]] std::optional<CharacterProblem> characterProblem(Player* player);

  std::vector<Match> tick(std::chrono::milliseconds elapsed);

  // Soloq arenas are keyed by battleground instance id until they end, so the
  // result can be fed back through resolveMatch.
  void                 registerMatch(uint32 bgInstanceId, Match const& match);
  std::optional<Match> takeMatch(uint32 bgInstanceId);

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

  // Total role counts cached from the last queue mutation; see refreshRoleCounts.
  // Cheap to read from the gossip handler.
  [[nodiscard]] RoleCounts const& roleCounts() const { return _roleCounts; }

  // Debug bypass for the item/enchant/talent/glyph readiness gate; see
  // characterProblem. Does not clear _validatedCharacters.
  void               setSkipCharacterChecks(bool value);
  [[nodiscard]] bool skipCharacterChecks() const { return _skipCharacterChecks; }

private:
  SoloqService() = default;

  void refreshRoleCounts();

  SoloqQueue                        _queue;
  std::unordered_map<uint32, Match> _pendingMatches;
  std::unordered_set<PlayerId>      _validatedCharacters;
  RoleCounts                        _roleCounts{};
  bool                              _skipCharacterChecks = false;
};
} // namespace arenacraft::soloq
