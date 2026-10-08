#pragma once

#include "Roles.hpp"
#include "Types.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
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

  // Flex mode relaxes the melee/caster/healer composition rule: a team may field
  // any two DPS (melee or caster) plus one healer instead of exactly one of each
  // role. Class stacking within a team is still rejected. Off by default.
  void               setFlexMode(bool value) { _flex = value; }
  [[nodiscard]] bool flexMode() const { return _flex; }

  struct QueueSnapshot
  {
    PlayerId                  id;
    Classes                   classId;
    uint8_t                   specIndex;
    Role                      role;
    uint32_t                  rating;
    uint32_t                  mmr;
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
    uint64_t minSequence;
  };

  [[nodiscard]] std::optional<Candidate> findBestMatch() const;
  [[nodiscard]] std::optional<Candidate> findStandardMatch() const;
  [[nodiscard]] std::optional<Candidate> findFlexMatch() const;
  [[nodiscard]] bool                     allCompatible(std::array<Entry const*, 6> const& six) const;
  [[nodiscard]] std::optional<Candidate> makeStandardCandidate(std::array<Entry const*, 6> const& six) const;
  [[nodiscard]] std::optional<Candidate> makeFlexCandidate(std::array<Entry const*, 6> const& six) const;
  [[nodiscard]] static uint32_t          windowFor(Entry const& entry);
  [[nodiscard]] static bool              compatible(Entry const& left, Entry const& right);

  static void fillStats(Candidate& candidate, std::array<Entry const*, 6> const& six);
  static bool betterThan(Candidate const& left, Candidate const& right);

  void removePlayers(std::array<PlayerId, 6> const& ids);

  std::vector<Entry> _entries;
  uint64_t           _nextSequence = 0;
  bool               _flex         = false;
};
} // namespace arenacraft::soloq
