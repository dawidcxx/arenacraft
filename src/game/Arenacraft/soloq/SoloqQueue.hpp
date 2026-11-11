#pragma once

#include "Roles.hpp"
#include "Types.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace arenacraft::soloq
{
class SoloqQueue
{
public:
  bool playerAddToQueue(QueuedPlayer player);
  bool playerRemoveFromQueue(PlayerId id);

  std::vector<Match> update(std::chrono::milliseconds elapsed);

  // Team composition is always two DPS (melee or caster) plus one healer. Class
  // stacking within a team is rejected, except Mancleave (see SoloqQueue.cpp).

  // Tourney mode flag. It is driven by the weekend schedule in TourneySchedule
  // (SoloqDriver refreshes it) and selects the tighter MMR search tuning in
  // windowFor: 100 initial / 300 cap / 30s growth instead of 200 / 600 / 15s.
  void               setTourneyMode(bool value) { _tourneyMode = value; }
  [[nodiscard]] bool tourneyMode() const { return _tourneyMode; }

  // Avoid list (#77): two players for whom this returns true are never put on the same team - partitions that would
  // do so are skipped like class stacking. Either side having avoided the other is enough. Unset = no avoiding.
  // The callback receives account ids, not character guids: avoiding is account-bound.
  using AvoidCheck = std::function<bool(uint32_t, uint32_t)>;
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
  struct Entry
  {
    QueuedPlayer              player;
    Role                      role;
    std::chrono::milliseconds waited;
    uint64_t                  sequence;
  };

  struct Candidate
  {
    Match    match;
    uint64_t imbalance;
    uint32_t spread;
    uint64_t waitTotal;
    uint64_t oldestSequence;
  };

  [[nodiscard]] std::optional<Candidate> findBestMatch() const;
  [[nodiscard]] bool                     allCompatible(std::array<Entry const*, 6> const& six) const;
  [[nodiscard]] std::optional<Candidate> makeCandidate(std::array<Entry const*, 6> const& six) const;
  [[nodiscard]] uint32_t                 windowFor(Entry const& entry) const;
  [[nodiscard]] bool                     compatible(Entry const& left, Entry const& right) const;
  [[nodiscard]] bool teamAvoids(QueuedPlayer const& x, QueuedPlayer const& y, QueuedPlayer const& z) const;

  static void fillStats(Candidate& candidate, std::array<Entry const*, 6> const& six);
  static bool betterThan(Candidate const& left, Candidate const& right);

  void removePlayers(std::array<PlayerId, 6> const& ids);

  std::vector<Entry> _entries;
  uint64_t           _nextSequence = 0;
  bool               _tourneyMode  = false;
  AvoidCheck         _avoids;
};
} // namespace arenacraft::soloq
