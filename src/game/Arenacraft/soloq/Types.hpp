#pragma once

#include "SharedDefines.h"

#include <chrono>
#include <cstdint>

namespace arenacraft::soloq
{
using PlayerId = uint64_t;

enum class Role : uint8_t
{
  Melee,
  Caster,
  Healer
};

struct QueuedPlayer
{
  PlayerId id;
  Classes  classId;
  uint8_t  specIndex;
  uint32_t rating;
  uint32_t mmr;
};

struct Team
{
  QueuedPlayer melee;
  QueuedPlayer caster;
  QueuedPlayer healer;
};

struct Match
{
  Team a;
  Team b;
};

namespace tuning
{
inline constexpr uint32_t InitialRating = 1400;
inline constexpr uint32_t InitialMmr    = 1500;
inline constexpr uint32_t MmrStep       = 50;
inline constexpr uint32_t MaxWindow     = 500;

inline constexpr std::chrono::seconds      StepInterval{30};
inline constexpr std::chrono::milliseconds StepIntervalMs =
    std::chrono::duration_cast<std::chrono::milliseconds>(StepInterval);
} // namespace tuning
} // namespace arenacraft::soloq
