#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace arenacraft::replay
{
// One server->client packet captured during a match.
struct RecordedPacket
{
  uint32_t             offsetMs = 0; // since recording start
  uint16_t             opcode   = 0;
  std::vector<uint8_t> data;

  bool operator==(RecordedPacket const&) const = default;
};

// Everything needed to play a match back. Stored zlib-compressed in
// arena_replay.data (see encode/decode).
struct ReplayData
{
  // server getMSTime() when recording started; movement packets carry server
  // timestamps that get shifted by (playback start - recordStartMs)
  uint32_t                    recordStartMs = 0;
  uint32_t                    mapId         = 0;
  float                       x = 0, y = 0, z = 0, o = 0; // viewer start position
  std::vector<RecordedPacket> packets;

  bool operator==(ReplayData const&) const = default;
};

std::vector<uint8_t>      encode(ReplayData const& replay);
std::optional<ReplayData> decode(std::vector<uint8_t> const& blob);

// Movement packets (MSG_MOVE_*) start with a packed guid followed by
// MovementInfo {u32 flags, u16 flags2, u32 time}. Returns the guid, or nullopt
// for a malformed packet.
std::optional<uint64_t> readPackedGuid(std::vector<uint8_t> const& data, std::size_t& pos);

// Adds `delta` to the MovementInfo time of a MSG_MOVE_* payload in place.
// Returns false (and leaves the payload untouched) if it is too short.
bool shiftMovementTime(std::vector<uint8_t>& data, uint32_t delta);
} // namespace arenacraft::replay
