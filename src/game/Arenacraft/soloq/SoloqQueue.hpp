#pragma once

#include "SoloqMatchmaker.hpp"
#include "Types.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace arenacraft::soloq
{
class SoloqQueue
{
public:
  SoloqQueue() : _matchmaker(makeMatchmaker(SoloqMode::Flex)) {}

  bool playerAddToQueue(QueuedPlayer player);
  bool playerRemoveFromQueue(PlayerId id);

  std::vector<Match> update(std::chrono::milliseconds elapsed);

  // Runtime matchmaker selection (flex by default; see SoloqMatchmaker.hpp).
  // Swapping the strategy does not touch the waiting pool: a caller that wants a
  // clean transition must clear() first (SoloqService::setMode does). Arenas
  // already formed are untouched - they live in SoloqService's pending map.
  void                    setMode(SoloqMode mode);
  [[nodiscard]] SoloqMode mode() const { return _matchmaker->mode(); }

  // Empties the waiting pool. Doesn't touch the real battleground queue or the
  // players' client badges; the caller must do that (see SoloqService::setMode).
  void clear();

  // True when the queued roles are enough for the current mode to pop.
  [[nodiscard]] bool requirementsMet(RoleCounts const& counts) const { return _matchmaker->requirementsMet(counts); }

  // Dry-runs the current matchmaker. A false result with requirementsMet(true)
  // means the MMR spread, class stacking or the avoid list blocks every match.
  [[nodiscard]] bool canFormMatch() const;

  // Tourney mode flag. It is driven by the weekend schedule in TourneySchedule
  // (SoloqDriver refreshes it) and selects the tighter MMR search tuning in
  // mmrWindowFor: 100 initial / 300 cap / 30s growth instead of 200 / 600 / 15s.
  void               setTourneyMode(bool value) { _tourneyMode = value; }
  [[nodiscard]] bool tourneyMode() const { return _tourneyMode; }

  // Avoid list (#77): two players for whom this returns true are never put on the same team - partitions that would
  // do so are skipped like class stacking. Either side having avoided the other is enough. Unset = no avoiding.
  // The callback receives account ids, not character guids: avoiding is account-bound.
  using AvoidCheck = arenacraft::soloq::AvoidCheck;
  void setAvoidCheck(AvoidCheck check) { _avoids = std::move(check); }

  struct QueueSnapshot
  {
    PlayerId id;
    Classes  classId;
    uint8_t  specIndex;
    Role     role;
    uint32_t rating;
    uint32_t mmr;
    // Effective MMR search window right now: the player accepts [mmr - mmrWindow, mmr + mmrWindow].
    uint32_t                  mmrWindow;
    std::chrono::milliseconds waited;
  };

  [[nodiscard]] std::size_t                size() const;
  [[nodiscard]] bool                       empty() const;
  [[nodiscard]] bool                       contains(PlayerId id) const;
  [[nodiscard]] std::vector<PlayerId>      waitingPlayers() const;
  [[nodiscard]] std::vector<QueueSnapshot> snapshot() const;

private:
  void removePlayers(std::array<PlayerId, 6> const& ids);

  std::vector<QueueEntry>          _entries;
  std::unique_ptr<SoloqMatchmaker> _matchmaker;
  uint64_t                         _nextSequence = 0;
  bool                             _tourneyMode  = false;
  AvoidCheck                       _avoids;
};
} // namespace arenacraft::soloq
