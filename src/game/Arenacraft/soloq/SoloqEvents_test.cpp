#include <doctest/doctest.h>

#include "SoloqEvents.hpp"

#include <string>

using arenacraft::soloq::buildMatchupEndedPayload;
using arenacraft::soloq::buildMatchupPayload;
using arenacraft::soloq::MatchupEndedEvent;
using arenacraft::soloq::MatchupEvent;
using arenacraft::soloq::ParticipantInfo;
using arenacraft::soloq::Role;

namespace
{
ParticipantInfo MakePlayer(uint64_t id, char const* name, uint32_t accountId, char const* account, Classes classId,
                           Role role, TeamId teamId, uint32_t rating, uint32_t mmr)
{
  return ParticipantInfo{id, name, accountId, account, classId, 0, role, teamId, rating, mmr};
}

MatchupEvent MakeEvent()
{
  MatchupEvent event{};
  event.bgInstanceId = 77;
  event.arenaType    = 3;
  event.mapId        = 617;
  event.bracketId    = 2;
  event.queueType    = 5;
  event.startedAtMs  = 1700000000000ULL;
  event.participants = {
      MakePlayer(0x1000, "Alpha", 1, "acc-alpha", CLASS_WARRIOR, Role::Melee, TEAM_ALLIANCE, 1500, 1550),
      MakePlayer(0x1001, "Bravo", 1, "acc-alpha", CLASS_MAGE, Role::Caster, TEAM_ALLIANCE, 1400, 1450),
      MakePlayer(0x1002, "Charlie", 2, "acc-bravo", CLASS_PRIEST, Role::Healer, TEAM_ALLIANCE, 1300, 1350),
      MakePlayer(0x2000, "Delta", 3, "acc-charlie", CLASS_ROGUE, Role::Melee, TEAM_HORDE, 1600, 1650),
      MakePlayer(0x2001, "Echo", 3, "acc-charlie", CLASS_WARLOCK, Role::Caster, TEAM_HORDE, 1200, 1250),
      MakePlayer(0x2002, "Foxtrot", 4, "acc-delta", CLASS_SHAMAN, Role::Healer, TEAM_HORDE, 1100, 1150),
  };
  return event;
}
} // namespace

TEST_CASE("buildMatchupPayload includes every participant with account and rating data")
{
  std::string const json = buildMatchupPayload(MakeEvent());

  CHECK(json.find("\"event\":\"soloq.matchup\"") != std::string::npos);
  CHECK(json.find("\"instanceId\":77") != std::string::npos);
  CHECK(json.find("\"mapId\":617") != std::string::npos);
  CHECK(json.find("\"startedAtMs\":1700000000000") != std::string::npos);

  for (char const* name : {"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot"})
    CHECK(json.find(std::string("\"characterName\":\"") + name + "\"") != std::string::npos);

  CHECK(json.find("\"accountName\":\"acc-alpha\"") != std::string::npos);
  CHECK(json.find("\"accountName\":\"acc-delta\"") != std::string::npos);

  CHECK(json.find("\"teamA\"") < json.find("\"teamB\""));
  CHECK(json.find("\"faction\":\"alliance\"") != std::string::npos);
  CHECK(json.find("\"faction\":\"horde\"") != std::string::npos);
}

TEST_CASE("buildMatchupPayload computes team averages and Elo deltas")
{
  std::string const json = buildMatchupPayload(MakeEvent());

  CHECK(json.find("\"averageRating\":1400") != std::string::npos); // (1500+1400+1300)/3
  CHECK(json.find("\"averageMmr\":1450") != std::string::npos);    // (1550+1450+1350)/3
  CHECK(json.find("\"averageRating\":1300") != std::string::npos); // (1600+1200+1100)/3
  CHECK(json.find("\"averageMmr\":1350") != std::string::npos);    // (1650+1250+1150)/3
  CHECK(json.find("\"winDelta\":") != std::string::npos);
  CHECK(json.find("\"lossDelta\":") != std::string::npos);
}

TEST_CASE("buildMatchupPayload escapes special characters in names")
{
  MatchupEvent event                  = MakeEvent();
  event.participants[0].characterName = "Quote\"Back\\Slash";
  std::string const json              = buildMatchupPayload(event);

  CHECK(json.find("\"characterName\":\"Quote\\\"Back\\\\Slash\"") != std::string::npos);
}

TEST_CASE("buildMatchupPayload emits empty strings for missing names")
{
  MatchupEvent event = MakeEvent();
  event.participants[0].characterName.clear();
  event.participants[0].accountName.clear();

  std::string const json = buildMatchupPayload(event);
  CHECK(json.find("\"characterName\":\"\"") != std::string::npos);
  CHECK(json.find("\"accountName\":\"\"") != std::string::npos);
}

TEST_CASE("buildMatchupEndedPayload reports instance, finished flag and players")
{
  MatchupEvent const source = MakeEvent();

  MatchupEndedEvent event{};
  event.bgInstanceId = source.bgInstanceId;
  event.finished     = false;
  event.participants = source.participants;

  std::string const json = buildMatchupEndedPayload(event);
  CHECK(json.find("\"event\":\"soloq.matchup.ended\"") != std::string::npos);
  CHECK(json.find("\"instanceId\":77") != std::string::npos);
  CHECK(json.find("\"finished\":false") != std::string::npos);
  CHECK(json.find("\"accountName\":\"acc-delta\"") != std::string::npos);

  event.finished = true;
  CHECK(buildMatchupEndedPayload(event).find("\"finished\":true") != std::string::npos);
}
