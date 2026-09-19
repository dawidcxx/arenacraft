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

  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] bool        empty() const;
  [[nodiscard]] bool        contains(PlayerId id) const;

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
    std::array<QueuedPlayer, 6> players;
    std::size_t                 partition;
    uint64_t                    imbalance;
    uint32_t                    spread;
    uint64_t                    waitTotal;
    uint64_t                    minSequence;
  };

  [[nodiscard]] std::optional<Candidate> findBestMatch() const;
  [[nodiscard]] static bool              allCompatible(std::array<Entry const*, 6> const& six);
  [[nodiscard]] static Candidate         makeCandidate(std::array<Entry const*, 6> const& six);
  [[nodiscard]] static Match             buildMatch(Candidate const& candidate);
  [[nodiscard]] static uint32_t          windowFor(Entry const& entry);
  [[nodiscard]] static bool              compatible(Entry const& left, Entry const& right);

  void removePlayers(std::array<PlayerId, 6> const& ids);

  std::vector<Entry> _entries;
  uint64_t           _nextSequence = 0;
};
} // namespace arenacraft::soloq
