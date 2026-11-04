#include <doctest/doctest.h>

#include "Opcodes.h"
#include "ReplayTimeline.hpp"

#include <algorithm>

using namespace arenacraft::replay;

namespace
{
std::vector<uint8_t> packedGuid(uint64_t guid)
{
  std::vector<uint8_t> out{0};
  for (int i = 0; i < 8; ++i)
  {
    uint8_t const byte = uint8_t(guid >> (i * 8));
    if (byte)
    {
      out[0] |= uint8_t(1 << i);
      out.push_back(byte);
    }
  }
  return out;
}

RecordedPacket packet(uint32_t offsetMs, uint16_t opcode, std::vector<uint8_t> data = {})
{
  return RecordedPacket{offsetMs, opcode, std::move(data)};
}

RecordedPacket move(uint32_t offsetMs, uint64_t guid)
{
  std::vector<uint8_t> data = packedGuid(guid);
  data.resize(data.size() + 10, 0); // MovementInfo flags/flags2/time
  return packet(offsetMs, MSG_MOVE_HEARTBEAT, std::move(data));
}
} // namespace

TEST_CASE("parseSeekTarget reads absolute and relative positions")
{
  CHECK(parseSeekTarget("95", 0, 300000) == 95000u);
  CHECK(parseSeekTarget("95s", 0, 300000) == 95000u);
  CHECK(parseSeekTarget("1:35", 0, 300000) == 95000u);
  CHECK(parseSeekTarget("+10", 50000, 300000) == 60000u);
  CHECK(parseSeekTarget("-10", 50000, 300000) == 40000u);
  CHECK(parseSeekTarget(" 0 ", 50000, 300000) == 0u);
}

TEST_CASE("parseSeekTarget clamps to the recording and rejects garbage")
{
  CHECK(parseSeekTarget("-60", 50000, 300000) == 0u);
  CHECK(parseSeekTarget("999", 0, 300000) == 300000u);
  CHECK_FALSE(parseSeekTarget("", 0, 1000).has_value());
  CHECK_FALSE(parseSeekTarget("abc", 0, 1000).has_value());
  CHECK_FALSE(parseSeekTarget("1:75", 0, 1000).has_value());
  CHECK_FALSE(parseSeekTarget("+", 0, 1000).has_value());
}

TEST_CASE("planJump keeps lasting packets, drops one-shot effects and keeps the last move per object")
{
  std::vector<RecordedPacket> const packets = {
      packet(0, SMSG_UPDATE_OBJECT),                   // 0 snapshot
      move(100, 0x10),                                 // 1
      packet(200, SMSG_SPELL_GO),                      // 2 transient
      move(300, 0x20),                                 // 3
      packet(400, SMSG_AURA_UPDATE, packedGuid(0x10)), // 4
      move(500, 0x10),                                 // 5 last move of 0x10
      packet(600, SMSG_PLAY_SPELL_VISUAL),             // 6 transient
      move(700, 0x20),                                 // 7 last move of 0x20 before the target
      packet(900, SMSG_UPDATE_OBJECT),                 // 8 after the target
  };

  std::size_t                    next = 0;
  std::vector<std::size_t> const plan = planJump(packets, 0, 800, next);
  CHECK(plan == std::vector<std::size_t>{0, 4, 5, 7});
  CHECK(next == 8);
}

TEST_CASE("planJump continues from the current packet")
{
  std::vector<RecordedPacket> const packets = {packet(0, SMSG_UPDATE_OBJECT), packet(100, SMSG_UPDATE_OBJECT),
                                               packet(200, SMSG_UPDATE_OBJECT)};
  std::size_t                       next    = 0;
  CHECK(planJump(packets, 1, 150, next) == std::vector<std::size_t>{1});
  CHECK(next == 2);
  CHECK(planJump(packets, 2, 50, next).empty());
  CHECK(next == 2);
}

TEST_CASE("collectObjectGuids finds movers, aura targets and destroyed objects")
{
  std::vector<uint8_t> destroy(9, 0);
  destroy[0]                                = 0x77; // little-endian guid 0x77
  std::vector<RecordedPacket> const packets = {move(0, 0x10), packet(10, SMSG_AURA_UPDATE, packedGuid(0x20)),
                                               packet(20, SMSG_DESTROY_OBJECT, destroy), move(30, 0x10),
                                               packet(40, SMSG_UPDATE_OBJECT)};

  std::vector<uint64_t> guids = collectObjectGuids(packets);
  std::sort(guids.begin(), guids.end());
  CHECK(guids == std::vector<uint64_t>{0x10, 0x20, 0x77});
}
