#pragma once

#include "Types.hpp"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace arenacraft::soloq
{
// Runtime-selectable composition rule for the solo queue.
//
//   Flex - each team fields any two DPS (melee or caster) plus one healer. The
//          default: the widest pool, the fastest pops.
//   Mch  - "melee / caster / healer": each team fields exactly one melee, one
//          caster and one healer.
//
// New modes are added by implementing SoloqMatchmaker below and adding the
// enum value to the factory in SoloqMatchmaker.cpp.
enum class SoloqMode : uint8_t
{
  Flex,
  Mch,
};

[[nodiscard]] std::string_view         modeName(SoloqMode mode);         // "flex" / "mch"
[[nodiscard]] std::optional<SoloqMode> parseMode(std::string_view name); // case-insensitive
[[nodiscard]] std::string_view         modeRequirement(SoloqMode mode);  // admin/status hint

// Avoid list check: true when the two accounts must never share a team.
using AvoidCheck = std::function<bool(uint32_t, uint32_t)>;

// Effective MMR search window right now: the player accepts
// [mmr - window, mmr + window]. Tourney mode uses the tighter tuning. Exposed
// for the queue's debug snapshot.
[[nodiscard]] uint32_t mmrWindowFor(QueueEntry const& entry, bool tourneyMode);

// A matchmaker decides which six queued players form a match and how those six
// split into two teams. Everything that does not depend on composition (MMR
// windows, MMR compatibility, the avoid list, fairness scoring) lives here in
// the base; a concrete mode only enumerates its legal six-sets/partitions.
class SoloqMatchmaker
{
public:
  virtual ~SoloqMatchmaker() = default;

  [[nodiscard]] virtual SoloqMode mode() const = 0;

  // True when the queued roles are enough to form a match under this mode.
  [[nodiscard]] virtual bool requirementsMet(RoleCounts const& counts) const = 0;

  // The best match available in the pool, or nullopt when none can form. Each
  // call considers the whole pool; the queue calls it again after removing the
  // six matched players, so one update can drain several matches.
  [[nodiscard]] virtual std::optional<Match> findBestMatch(std::vector<QueueEntry> const& entries, bool tourneyMode,
                                                           AvoidCheck const& avoids) const = 0;

protected:
  struct Candidate
  {
    Match    match;
    uint64_t imbalance      = 0;
    uint32_t spread         = 0;
    uint64_t waitTotal      = 0;
    uint64_t oldestSequence = 0;
  };

  // gap <= max(window(left), window(right)); windows are not summed.
  [[nodiscard]] static bool compatible(QueueEntry const& left, QueueEntry const& right, bool tourneyMode);
  [[nodiscard]] static bool allCompatible(std::array<QueueEntry const*, 6> const& six, bool tourneyMode);
  [[nodiscard]] static bool teamAvoids(QueuedPlayer const& x, QueuedPlayer const& y, QueuedPlayer const& z,
                                       AvoidCheck const& avoids);

  static void fillStats(Candidate& candidate, std::array<QueueEntry const*, 6> const& six);
  // Queue order dominates, then total wait, then team imbalance, then MMR spread.
  static bool betterThan(Candidate const& left, Candidate const& right);
};

// Builds the implementation for `mode`. Flex is the default at construction.
std::unique_ptr<SoloqMatchmaker> makeMatchmaker(SoloqMode mode);
} // namespace arenacraft::soloq
