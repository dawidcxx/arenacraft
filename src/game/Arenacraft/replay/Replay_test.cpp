#include <doctest/doctest.h>

#include "ReplayConfig.hpp"
#include "ReplayFormat.hpp"

#include <cstring>

using namespace arenacraft::replay;

TEST_CASE("parseBrackets reads the comma separated bracket list")
{
  CHECK(parseBrackets("2v2") == Brackets{.rated2v2 = true});
  CHECK(parseBrackets(" 3v3 , SoloQ ") == Brackets{.rated3v3 = true, .soloq = true});
  CHECK(parseBrackets("2s,3s,5s") == Brackets{.rated2v2 = true, .rated3v3 = true, .rated5v5 = true});
  CHECK(parseBrackets("skirmish") == Brackets{.skirmish = true});
  CHECK(parseBrackets("all") == Brackets{true, true, true, true, true});
  CHECK(parseBrackets("") == Brackets{});

  std::string unknown;
  CHECK(parseBrackets("2v2,4v4,1v1", &unknown) == Brackets{.rated2v2 = true});
  CHECK(unknown == "4v4");
}

TEST_CASE("Config::records follows the bracket switches")
{
  Config cfg;
  cfg.brackets = parseBrackets("soloq");
  CHECK(cfg.records(Bracket::Soloq));
  CHECK_FALSE(cfg.records(Bracket::Rated3v3));
  CHECK_FALSE(cfg.records(Bracket::Skirmish));
}

TEST_CASE("replay data round-trips through encode/decode")
{
  ReplayData in;
  in.recordStartMs = 123456;
  in.mapId         = 572;
  in.x             = 1285.5f;
  in.y             = 1667.25f;
  in.z             = 39.5f;
  in.o             = 3.14f;
  in.packets.push_back({0, 0xA9, {1, 2, 3}});
  in.packets.push_back({15, 0xEE, {}});
  in.packets.push_back({90000, 0x131, std::vector<uint8_t>(5000, 0x7F)});

  std::vector<uint8_t> const blob = encode(in);
  REQUIRE_FALSE(blob.empty());

  std::optional<ReplayData> const out = decode(blob);
  REQUIRE(out);
  CHECK(*out == in);
}

TEST_CASE("decode rejects corrupt blobs")
{
  ReplayData in;
  in.packets.push_back({1, 2, {3, 4}});
  std::vector<uint8_t> blob = encode(in);

  CHECK_FALSE(decode({}));
  std::vector<uint8_t> truncated(blob.begin(), blob.begin() + blob.size() / 2);
  CHECK_FALSE(decode(truncated));
  blob[0] ^= 0xFF;
  CHECK_FALSE(decode(blob));
}

TEST_CASE("readPackedGuid and shiftMovementTime handle MSG_MOVE payloads")
{
  // guid 0x0102 packed: mask 0b11, bytes 02 01; then flags, flags2, time=1000
  std::vector<uint8_t> data = {0x03, 0x02, 0x01, 0, 0, 0, 0, 0, 0};
  uint32_t             time = 1000;
  data.resize(data.size() + 4);
  std::memcpy(data.data() + 9, &time, 4);

  std::size_t pos = 0;
  CHECK(readPackedGuid(data, pos) == 0x0102u);
  CHECK(pos == 3);

  REQUIRE(shiftMovementTime(data, 500));
  std::memcpy(&time, data.data() + 9, 4);
  CHECK(time == 1500);

  std::vector<uint8_t> shortData = {0x01, 0x05, 0, 0};
  CHECK_FALSE(shiftMovementTime(shortData, 1));
}
