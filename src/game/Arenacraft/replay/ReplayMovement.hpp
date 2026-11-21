#pragma once

#include "Position.h"
#include "ReplayFormat.hpp"
#include "WorldPacket.h"

#include <map>

namespace arenacraft::replay
{
struct GroundSegment
{
  uint64_t guid;
  uint32_t beginMs;
  uint32_t endMs;
  uint32_t flags;
  Position from;
  Position to;

  [[nodiscard]] Position positionAt(uint32_t timeMs) const;
};

// Indexed by the packet replaced at the beginning of each supported interval.
using GroundSegments = std::map<std::size_t, GroundSegment>;
GroundSegments buildGroundSegments(std::vector<RecordedPacket> const& packets);

WorldPacket groundAnchor(GroundSegment const& segment, uint32_t timeMs, uint32_t serverTime);
WorldPacket groundSpline(GroundSegment const& segment, uint32_t timeMs, uint32_t splineId);
WorldPacket groundStop(GroundSegment const& segment, uint32_t timeMs, uint32_t splineId);
} // namespace arenacraft::replay
