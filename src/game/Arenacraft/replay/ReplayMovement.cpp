#include "ReplayMovement.hpp"

#include "MoveSplineFlag.h"
#include "MovementPacketBuilder.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "ReplayRecorder.hpp"
#include "UnitDefines.h"
#include "UpdateData.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace arenacraft::replay
{
namespace
{
struct Sample
{
  uint64_t guid;
  uint32_t flags;
  uint32_t time;
  Position position;
};

bool positionalOpcode(uint16_t opcode)
{
  return opcode == MSG_MOVE_HEARTBEAT || opcode == MSG_MOVE_START_FORWARD || opcode == MSG_MOVE_START_BACKWARD ||
         opcode == MSG_MOVE_STOP || opcode == MSG_MOVE_SET_FACING || opcode == MSG_MOVE_STOP_TURN;
}

bool splineStateOpcode(uint16_t opcode)
{
  switch (opcode)
  {
  case SMSG_SPLINE_MOVE_FEATHER_FALL:
  case SMSG_SPLINE_MOVE_GRAVITY_DISABLE:
  case SMSG_SPLINE_MOVE_GRAVITY_ENABLE:
  case SMSG_SPLINE_MOVE_LAND_WALK:
  case SMSG_SPLINE_MOVE_NORMAL_FALL:
  case SMSG_SPLINE_MOVE_ROOT:
  case SMSG_SPLINE_MOVE_SET_FLYING:
  case SMSG_SPLINE_MOVE_SET_HOVER:
  case SMSG_SPLINE_MOVE_SET_RUN_MODE:
  case SMSG_SPLINE_MOVE_SET_WALK_MODE:
  case SMSG_SPLINE_MOVE_START_SWIM:
  case SMSG_SPLINE_MOVE_STOP_SWIM:
  case SMSG_SPLINE_MOVE_UNROOT:
  case SMSG_SPLINE_MOVE_UNSET_FLYING:
  case SMSG_SPLINE_MOVE_UNSET_HOVER:
  case SMSG_SPLINE_MOVE_WATER_WALK:
  case SMSG_SPLINE_SET_FLIGHT_BACK_SPEED:
  case SMSG_SPLINE_SET_FLIGHT_SPEED:
  case SMSG_SPLINE_SET_PITCH_RATE:
  case SMSG_SPLINE_SET_RUN_BACK_SPEED:
  case SMSG_SPLINE_SET_RUN_SPEED:
  case SMSG_SPLINE_SET_SWIM_BACK_SPEED:
  case SMSG_SPLINE_SET_SWIM_SPEED:
  case SMSG_SPLINE_SET_TURN_RATE:
  case SMSG_SPLINE_SET_WALK_SPEED:
    return true;
  default:
    return false;
  }
}

std::optional<Sample> sampleOf(RecordedPacket const& packet)
{
  if (!positionalOpcode(packet.opcode))
    return std::nullopt;

  std::size_t pos  = 0;
  auto        guid = readPackedGuid(packet.data, pos);
  // Only the fixed-size, grounded MovementInfo layout is eligible.
  if (!guid || !ObjectGuid(*guid).IsPlayer() || packet.data.size() != pos + 30)
    return std::nullopt;

  ByteBuffer data;
  data.append(packet.data.data() + pos, 30);
  Sample sample;
  uint16 flags2;
  sample.guid = *guid;
  data >> sample.flags >> flags2 >> sample.time >> sample.position.PositionXYZOStream();
  constexpr uint32 allowed = MOVEMENTFLAG_FORWARD | MOVEMENTFLAG_BACKWARD | MOVEMENTFLAG_WALKING;
  if (flags2 || (sample.flags & ~allowed) ||
      (sample.flags & (MOVEMENTFLAG_FORWARD | MOVEMENTFLAG_BACKWARD)) ==
          (MOVEMENTFLAG_FORWARD | MOVEMENTFLAG_BACKWARD) ||
      !sample.position.IsPositionValid())
    return std::nullopt;
  return sample;
}

// Ordinary value changes do not interrupt movement. Treat creates, visibility changes,
// movement blocks and compressed/unknown layouts as barriers rather than guessing their state.
bool valuesOnly(RecordedPacket const& packet)
{
  if (packet.opcode != SMSG_UPDATE_OBJECT || packet.data.size() < 4)
    return false;
  auto const& data   = packet.data;
  std::size_t pos    = 0;
  auto        read32 = [&]()
  {
    uint32 value;
    std::memcpy(&value, data.data() + pos, 4);
    pos += 4;
    return value;
  };
  uint32 count = read32();
  for (uint32 i = 0; i < count; ++i)
  {
    if (pos >= data.size() || data[pos++] != UPDATETYPE_VALUES || !readPackedGuid(data, pos) || pos >= data.size())
      return false;
    uint8 blocks = data[pos++];
    if (data.size() - pos < std::size_t(blocks) * 4)
      return false;
    uint32 fields = 0;
    for (uint8 block = 0; block < blocks; ++block)
      fields += std::popcount(read32());
    if (data.size() - pos < std::size_t(fields) * 4)
      return false;
    pos += std::size_t(fields) * 4;
  }
  return pos == data.size();
}
} // namespace

Position GroundSegment::positionAt(uint32_t timeMs) const
{
  float fraction = float(std::clamp(timeMs, beginMs, endMs) - beginMs) / float(endMs - beginMs);
  return Position(from.GetPositionX() + fraction * (to.GetPositionX() - from.GetPositionX()),
                  from.GetPositionY() + fraction * (to.GetPositionY() - from.GetPositionY()),
                  from.GetPositionZ() + fraction * (to.GetPositionZ() - from.GetPositionZ()), from.GetOrientation());
}

GroundSegments buildGroundSegments(std::vector<RecordedPacket> const& packets)
{
  GroundSegments                            segments;
  std::unordered_map<uint64_t, std::size_t> pending;
  for (std::size_t index = 0; index < packets.size(); ++index)
  {
    auto const& packet = packets[index];
    if (packet.opcode == SMSG_UPDATE_OBJECT || packet.opcode == SMSG_COMPRESSED_UPDATE_OBJECT ||
        packet.opcode == SMSG_DESTROY_OBJECT || packet.opcode == SMSG_COMPRESSED_MOVES)
    {
      if (!valuesOnly(packet))
        pending.clear();
      continue;
    }
    if (!isMovementOpcode(packet.opcode) && packet.opcode != SMSG_MONSTER_MOVE &&
        packet.opcode != SMSG_MONSTER_MOVE_TRANSPORT && !splineStateOpcode(packet.opcode))
      continue;

    std::size_t pos  = 0;
    auto        guid = readPackedGuid(packet.data, pos);
    if (!guid)
    {
      pending.clear();
      continue;
    }
    auto sample   = sampleOf(packet);
    auto previous = pending.find(*guid);
    if (sample && previous != pending.end())
    {
      auto const& startPacket    = packets[previous->second];
      auto        start          = sampleOf(startPacket);
      uint32      duration       = packet.offsetMs - startPacket.offsetMs;
      uint32      sampleDuration = sample->time - start->time;
      float       dx             = sample->position.GetPositionX() - start->position.GetPositionX();
      float       dy             = sample->position.GetPositionY() - start->position.GetPositionY();
      float       dz             = sample->position.GetPositionZ() - start->position.GetPositionZ();
      float       distance       = std::hypot(dx, dy);
      float       angleError = std::remainder(std::atan2(dy, dx) - start->position.GetOrientation(), float(2 * M_PI));
      // Preserve dispatch-time alignment with combat. Do not turn packet bursts, gaps,
      // elevation changes or unexplained displacement into a fabricated path.
      if ((start->flags & MOVEMENTFLAG_FORWARD) && duration >= 20 && duration <= 1500 && sampleDuration > 0 &&
          std::abs(int64(sampleDuration) - duration) <= 100 && distance >= 0.01f &&
          distance <= 16.0f * float(duration) / 1000.0f && std::abs(dz) <= 0.5f && std::abs(angleError) <= 0.1f)
        segments.emplace(previous->second, GroundSegment{*guid, startPacket.offsetMs, packet.offsetMs, start->flags,
                                                         start->position, sample->position});
    }
    pending.erase(*guid);
    if (sample)
      pending.emplace(*guid, index);
  }
  return segments;
}

WorldPacket groundAnchor(GroundSegment const& segment, uint32_t timeMs, uint32_t serverTime)
{
  WorldPacket packet(MSG_MOVE_HEARTBEAT);
  Position    position = segment.positionAt(timeMs);
  packet << ObjectGuid(segment.guid).WriteAsPacked();
  packet << uint32(segment.flags & MOVEMENTFLAG_WALKING) << uint16(0) << serverTime;
  packet << position.PositionXYZOStream() << uint32(0);
  return packet;
}

WorldPacket groundSpline(GroundSegment const& segment, uint32_t timeMs, uint32_t splineId)
{
  WorldPacket packet(SMSG_MONSTER_MOVE);
  Position    position = segment.positionAt(timeMs);
  uint32      flags    = Movement::MoveSplineFlag::OrientationFixed;
  if (segment.flags & MOVEMENTFLAG_WALKING)
    flags |= Movement::MoveSplineFlag::Walkmode;
  packet << ObjectGuid(segment.guid).WriteAsPacked();
  packet << uint8(0) << position.PositionXYZStream() << splineId << uint8(0);
  packet << flags << uint32(segment.endMs - std::clamp(timeMs, segment.beginMs, segment.endMs));
  packet << uint32(1) << segment.to.GetPositionX() << segment.to.GetPositionY() << segment.to.GetPositionZ();
  return packet;
}

WorldPacket groundStop(GroundSegment const& segment, uint32_t timeMs, uint32_t splineId)
{
  WorldPacket packet(SMSG_MONSTER_MOVE);
  Position    position = segment.positionAt(timeMs);
  packet << ObjectGuid(segment.guid).WriteAsPacked();
  Movement::PacketBuilder::WriteStopMovement(
      G3D::Vector3(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ()), splineId, packet);
  return packet;
}
} // namespace arenacraft::replay
