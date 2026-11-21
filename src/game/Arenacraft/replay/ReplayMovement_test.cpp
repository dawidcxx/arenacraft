#include <doctest/doctest.h>

#include "MoveSplineFlag.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "ReplayMovement.hpp"
#include "UnitDefines.h"
#include "UpdateData.h"
#include <cmath>
#include <cstring>

using namespace arenacraft::replay;

namespace
{
RecordedPacket recorded(uint32_t at, WorldPacket const& packet)
{
  return {at, packet.GetOpcode(), {packet.contents(), packet.contents() + packet.size()}};
}

RecordedPacket move(uint32_t at, float x, uint32 flags = MOVEMENTFLAG_FORWARD, uint16 opcode = MSG_MOVE_HEARTBEAT,
                    float y = 0, float z = 0, float facing = 0)
{
  WorldPacket packet(opcode);
  packet << ObjectGuid(uint64(42)).WriteAsPacked() << flags << uint16(0) << uint32(at + 10000);
  packet << x << y << z << facing << uint32(0);
  return recorded(at, packet);
}
} // namespace

TEST_CASE("Replay ground movement reconstructs heartbeat intervals without modifying recorded packets")
{
  std::vector<RecordedPacket> packets  = {move(0, 0), move(500, 3.5f), move(1000, 7, 0, MSG_MOVE_STOP)};
  auto                        original = packets;
  auto                        segments = buildGroundSegments(packets);
  REQUIRE(segments.size() == 2);
  CHECK(packets == original);
  CHECK(segments.at(0).positionAt(250).GetPositionX() == doctest::Approx(1.75f));
  CHECK(segments.at(1).positionAt(750).GetPositionX() == doctest::Approx(5.25f));
  CHECK(segments.at(0).positionAt(2000).GetPositionX() == doctest::Approx(3.5f));
}

TEST_CASE("Replay ground movement leaves backward movement strafing and turning raw")
{
  auto backward = buildGroundSegments({move(0, 0, MOVEMENTFLAG_BACKWARD), move(500, -2, MOVEMENTFLAG_BACKWARD)});
  CHECK(backward.empty());
  CHECK(buildGroundSegments(
            {move(0, 0, MOVEMENTFLAG_STRAFE_LEFT), move(500, 0, MOVEMENTFLAG_STRAFE_LEFT, MSG_MOVE_HEARTBEAT, 3.5f)})
            .empty());
  CHECK(buildGroundSegments({move(0, 0, MOVEMENTFLAG_FORWARD | MOVEMENTFLAG_LEFT), move(500, 3.5f)}).empty());
  CHECK(buildGroundSegments({move(0, 0), move(500, 0, MOVEMENTFLAG_FORWARD, MSG_MOVE_HEARTBEAT, 3.5f)}).empty());
}

TEST_CASE("Replay ground movement keeps recorded facing rather than aligning the model to its spline")
{
  float facing = 1.0f;
  auto  segments =
      buildGroundSegments({move(0, 0, MOVEMENTFLAG_FORWARD | MOVEMENTFLAG_WALKING, MSG_MOVE_HEARTBEAT, 0, 0, facing),
                           move(500, std::cos(facing) * 1.25f, MOVEMENTFLAG_FORWARD | MOVEMENTFLAG_WALKING,
                                MSG_MOVE_HEARTBEAT, std::sin(facing) * 1.25f, 0, facing + 0.05f)});
  REQUIRE(segments.size() == 1);
  auto const& segment = segments.at(0);
  CHECK(segment.positionAt(250).GetOrientation() == doctest::Approx(facing));
  auto       packet = groundSpline(segment, 250, 1);
  ObjectGuid guid;
  uint8      unknown, type;
  uint32     id, flags;
  Position   position;
  packet >> guid.ReadAsPacked() >> unknown >> position.PositionXYZStream() >> id >> type >> flags;
  CHECK(flags == (Movement::MoveSplineFlag::OrientationFixed | Movement::MoveSplineFlag::Walkmode));
}

TEST_CASE("Replay ground movement falls back across gaps bursts teleports jumps and state changes")
{
  for (auto const& end :
       {move(5000, 35), move(0, 3.5f), move(500, 100), move(500, 3.5f, MOVEMENTFLAG_FORWARD, MSG_MOVE_TELEPORT),
        move(500, 3.5f, MOVEMENTFLAG_FALLING, MSG_MOVE_JUMP), move(500, 3.5f, MOVEMENTFLAG_ROOT, MSG_MOVE_ROOT),
        move(500, 3.5f, MOVEMENTFLAG_FORWARD, MSG_MOVE_SET_RUN_SPEED),
        move(500, 3.5f, MOVEMENTFLAG_FORWARD, MSG_MOVE_HEARTBEAT, 0, 3)})
    CHECK(buildGroundSegments({move(0, 0), end}).empty());

  for (uint16 opcode : {SMSG_MONSTER_MOVE, SMSG_SPLINE_MOVE_ROOT, SMSG_SPLINE_SET_RUN_SPEED})
  {
    WorldPacket packet(opcode);
    packet << ObjectGuid(uint64(42)).WriteAsPacked();
    CHECK(buildGroundSegments({move(0, 0), recorded(100, packet), move(500, 3.5f)}).empty());
  }
  auto burst     = move(500, 3.5f);
  burst.offsetMs = 20;
  CHECK(buildGroundSegments({move(0, 0), burst}).empty());
}

TEST_CASE("Replay ground movement tolerates value updates but not visibility and unknown update blocks")
{
  WorldPacket values(SMSG_UPDATE_OBJECT);
  values << uint32(1) << uint8(UPDATETYPE_VALUES) << ObjectGuid(uint64(42)).WriteAsPacked();
  values << uint8(1) << uint32(1) << uint32(123);
  CHECK(buildGroundSegments({move(0, 0), recorded(100, values), move(500, 3.5f)}).size() == 1);

  WorldPacket hidden(SMSG_UPDATE_OBJECT);
  hidden << uint32(1) << uint8(UPDATETYPE_OUT_OF_RANGE_OBJECTS) << uint32(1);
  hidden << ObjectGuid(uint64(42)).WriteAsPacked();
  CHECK(buildGroundSegments({move(0, 0), recorded(100, hidden), move(500, 3.5f)}).empty());

  values.resize(values.size() - 1);
  CHECK(buildGroundSegments({move(0, 0), recorded(100, values), move(500, 3.5f)}).empty());
}

TEST_CASE("Replay ground movement resumes and seeks with only the remaining spline duration")
{
  auto segments = buildGroundSegments({move(0, 0), move(500, 3.5f)});
  REQUIRE(segments.size() == 1);
  auto const& segment = segments.at(0);
  auto        packet  = groundSpline(segment, 250, 99);
  ObjectGuid  guid;
  uint8       unknown, type;
  uint32      id, flags, duration, count;
  Position    start, end;
  packet >> guid.ReadAsPacked() >> unknown >> start.PositionXYZStream() >> id >> type;
  packet >> flags >> duration >> count >> end.PositionXYZStream();
  CHECK(guid == ObjectGuid(uint64(42)));
  CHECK(id == 99);
  CHECK(type == 0);
  CHECK(flags == Movement::MoveSplineFlag::OrientationFixed);
  CHECK(duration == 250);
  CHECK(count == 1);
  CHECK(start.GetPositionX() == doctest::Approx(1.75f));
  CHECK(end.GetPositionX() == doctest::Approx(3.5f));
  CHECK(packet.rpos() == packet.size());

  auto stop = groundStop(segment, 250, 100);
  stop >> guid.ReadAsPacked() >> unknown >> start.PositionXYZStream() >> id >> type;
  CHECK(type == 1);
  CHECK(start.GetPositionX() == doctest::Approx(1.75f));
  CHECK(stop.rpos() == stop.size());

  auto   anchor = groundAnchor(segment, 250, 12345);
  uint16 flags2;
  uint32 time, fallTime;
  anchor >> guid.ReadAsPacked() >> flags >> flags2 >> time >> start.PositionXYZOStream() >> fallTime;
  CHECK(flags == 0);
  CHECK(time == 12345);
  CHECK(start.GetPositionX() == doctest::Approx(1.75f));
  CHECK(anchor.rpos() == anchor.size());
}

TEST_CASE("Replay ground movement rejects truncated samples and accepts wrapping movement timestamps")
{
  auto        start = move(0, 0);
  auto        end   = move(500, 3.5f);
  std::size_t pos   = 0;
  REQUIRE(readPackedGuid(start.data, pos).has_value());
  uint32 startTime = 0xFFFFFF00u;
  uint32 endTime   = startTime + 500;
  std::memcpy(start.data.data() + pos + 6, &startTime, 4);
  std::memcpy(end.data.data() + pos + 6, &endTime, 4);
  CHECK(buildGroundSegments({start, end}).size() == 1);
  std::memcpy(end.data.data() + pos + 6, &startTime, 4);
  CHECK(buildGroundSegments({start, end}).empty());
  end.data.pop_back();
  CHECK(buildGroundSegments({start, end}).empty());
}
