
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "soloq/soloq_match_maker.hpp"
#include <algorithm>

// #define BENCHMARK 1

TEST_CASE("MatchMaker scenario test suite")
{
#ifndef BENCHMARK
    SUBCASE("The most basic example should generate a valid match")
    {
        arenacraft::soloq::MatchMaker matchmaker(100);

        arenacraft::soloq::SoloqPlayer player1{1, arenacraft::CLASS_WARRIOR, 1001, 0}; // arms warrior - melee
        arenacraft::soloq::SoloqPlayer player3{2, arenacraft::CLASS_WARLOCK, 1080, 0}; // affliction warlock - caster
        arenacraft::soloq::SoloqPlayer player2{3, arenacraft::CLASS_PALADIN, 1020, 0}; // holy paladin - healer

        arenacraft::soloq::SoloqPlayer player4{4, arenacraft::CLASS_DEATH_KNIGHT, 1010, 2}; // unholy dk - melee
        arenacraft::soloq::SoloqPlayer player6{5, arenacraft::CLASS_SHAMAN, 1002, 0};       // elemental shaman - caster
        arenacraft::soloq::SoloqPlayer player5{6, arenacraft::CLASS_PRIEST, 1090, 1};       // holy priest - healer

        matchmaker.AddPlayer(player1);
        matchmaker.AddPlayer(player2);
        matchmaker.AddPlayer(player3);
        matchmaker.AddPlayer(player4);
        matchmaker.AddPlayer(player5);
        matchmaker.AddPlayer(player6);

        CHECK_MESSAGE(
            matchmaker.GetInfo().GetTotalPlayerCount() == 6,
            "There should be 6 players in Queue, got " << matchmaker.GetInfo().GetTotalPlayerCount());
        CHECK_MESSAGE(matchmaker.GetInfo().classToCount[arenacraft::CLASS_WARRIOR] == 1, "There should be a arenacraft::CLASS_WARRIOR in queue");

        auto matchups = matchmaker.PopMatchups();
        std::cout << "Matchups count: " << matchups.size() << std::endl;
        for (auto matchup : matchups)
        {
            std::cout << "Basic example Matchup: " << matchup << std::endl;
        }
        CHECK_MESSAGE(matchups.size() == 1, "Expected 1 matchup, got " << matchups.size());

        auto matchup = matchups[0];
        auto team1 = matchup.team1;
        auto team2 = matchup.team2;

        CHECK_MESSAGE(team1.size() == 3, "Expected 3 players in team1, got " << team1.size());
        CHECK_MESSAGE(team2.size() == 3, "Expected 3 players in team2, got " << team2.size());

        CHECK_MESSAGE(std::find(team1.begin(), team1.end(), player1) != team1.end(), "Player1 not found in team1");
        CHECK_MESSAGE(std::find(team1.begin(), team1.end(), player2) != team1.end(), "Player2 not found in team1");
        CHECK_MESSAGE(std::find(team1.begin(), team1.end(), player3) != team1.end(), "Player3 not found in team1");

        CHECK_MESSAGE(std::find(team2.begin(), team2.end(), player4) != team2.end(), "Player4 not found in team2");
        CHECK_MESSAGE(std::find(team2.begin(), team2.end(), player5) != team2.end(), "Player5 not found in team2");
        CHECK_MESSAGE(std::find(team2.begin(), team2.end(), player6) != team2.end(), "Player6 not found in team2");

        CHECK_MESSAGE(matchmaker.GetInfo().GetTotalPlayerCount() == 0, "Queue needs to be cleared after calling .PopMatchups()");
        CHECK_MESSAGE(matchmaker.GetInfo().classToCount[arenacraft::CLASS_WARRIOR] == 0, "arenacraft::CLASS_WARRIOR should be removed from queue");
    }

    SUBCASE("Non MCH queues should not pop")
    {
        arenacraft::soloq::MatchMaker matchmaker(100);

        arenacraft::soloq::SoloqPlayer player1{1, arenacraft::CLASS_WARRIOR, 1001, 0}; // arms warrior - melee
        arenacraft::soloq::SoloqPlayer player2{3, arenacraft::CLASS_PALADIN, 1020, 0}; // holy paladin - healera
        arenacraft::soloq::SoloqPlayer player3{2, arenacraft::CLASS_ROGUE, 1080, 0};   // rogue assasination - melee

        arenacraft::soloq::SoloqPlayer player4{4, arenacraft::CLASS_DEATH_KNIGHT, 1010, 2}; // unholy dk - melee
        arenacraft::soloq::SoloqPlayer player6{5, arenacraft::CLASS_SHAMAN, 1002, 0};       // elemental shaman - caster
        arenacraft::soloq::SoloqPlayer player5{6, arenacraft::CLASS_PRIEST, 1090, 1};       // holy priest - healer

        matchmaker.AddPlayer(player1);
        matchmaker.AddPlayer(player2);
        matchmaker.AddPlayer(player3);
        matchmaker.AddPlayer(player4);
        matchmaker.AddPlayer(player5);
        matchmaker.AddPlayer(player6);

        CHECK_MESSAGE(
            matchmaker.GetInfo().GetTotalPlayerCount() == 6,
            "There should be 6 players in Queue, got " << matchmaker.GetInfo().GetTotalPlayerCount());
        CHECK_MESSAGE(matchmaker.GetInfo().meleeCount == 3, "There should be 3 melee in queue");

        auto matchups = matchmaker.PopMatchups();

        for (auto matchup : matchups)
        {
            std::cout << "Non MCH queues should not pop Matchup: " << matchup << std::endl;
        }

        CHECK_MESSAGE(
            matchmaker.GetInfo().GetTotalPlayerCount() == 6,
            "Queue should not be cleared if there are no MCH matchups");
        CHECK_MESSAGE(matchups.size() == 0, "Expected 0 matchups, got " << matchups.size());
    }

    SUBCASE("Prevent class stacking")
    {
        arenacraft::soloq::MatchMaker matchmaker(100);

        arenacraft::soloq::SoloqPlayer player1{1, arenacraft::CLASS_PALADIN, 1001, 2}; // ret paladin        - melee
        arenacraft::soloq::SoloqPlayer player3{2, arenacraft::CLASS_WARLOCK, 1080, 0}; // affliction warlock - caster
        arenacraft::soloq::SoloqPlayer player2{3, arenacraft::CLASS_PALADIN, 1020, 0}; // holy paladin       - healer

        arenacraft::soloq::SoloqPlayer player4{4, arenacraft::CLASS_WARRIOR, 1010, 0}; // arms warrior       - melee
        arenacraft::soloq::SoloqPlayer player6{5, arenacraft::CLASS_SHAMAN, 1002, 0};  // elemental shaman   - caster
        arenacraft::soloq::SoloqPlayer player5{6, arenacraft::CLASS_PALADIN, 1090, 0}; // holy paladin       - healer

        matchmaker.AddPlayer(player1);
        matchmaker.AddPlayer(player2);
        matchmaker.AddPlayer(player3);
        matchmaker.AddPlayer(player4);
        matchmaker.AddPlayer(player5);
        matchmaker.AddPlayer(player6);

        CHECK_MESSAGE(
            matchmaker.GetInfo().GetTotalPlayerCount() == 6,
            "There should be 6 players in Queue, got " << matchmaker.GetInfo().GetTotalPlayerCount());
        CHECK_MESSAGE(matchmaker.GetInfo().classToCount[arenacraft::CLASS_PALADIN] == 3, "There should be 3 paladins in queue");

        auto matchups = matchmaker.PopMatchups();

        for (auto matchup : matchups)
        {
            std::cout << "Prevent class stacking Matchup: " << matchup << std::endl;
        }

        CHECK_MESSAGE(
            matchmaker.GetInfo().GetTotalPlayerCount() == 6,
            "Queue should not be cleared if there are no MCH matchups");
        CHECK_MESSAGE(matchups.size() == 0, "Expected 0 matchups, got " << matchups.size());
    }

    SUBCASE("Queue player mmr extension should work properly")
    {
        arenacraft::soloq::MatchMaker matchmaker(100);

        arenacraft::soloq::SoloqPlayer player1{1, arenacraft::CLASS_WARRIOR, 1001, 0};      // arms warrior       - melee
        arenacraft::soloq::SoloqPlayer player3{2, arenacraft::CLASS_WARLOCK, 1080, 0};      // affliction warlock - caster
        arenacraft::soloq::SoloqPlayer player2{3, arenacraft::CLASS_PALADIN, 1400, 0};      // holy paladin       - healer  <= High rated player
        arenacraft::soloq::SoloqPlayer player4{4, arenacraft::CLASS_DEATH_KNIGHT, 1010, 2}; // unholy dk          - melee
        arenacraft::soloq::SoloqPlayer player6{5, arenacraft::CLASS_SHAMAN, 1002, 0};       // elemental shaman   - caster
        arenacraft::soloq::SoloqPlayer player5{6, arenacraft::CLASS_PRIEST, 1090, 1};       // holy priest        - healer

        matchmaker.AddPlayer(player1);
        matchmaker.AddPlayer(player2);
        matchmaker.AddPlayer(player3);
        matchmaker.AddPlayer(player4);
        matchmaker.AddPlayer(player5);
        matchmaker.AddPlayer(player6);

        CHECK_MESSAGE(
            matchmaker.GetInfo().GetTotalPlayerCount() == 6,
            "There should be 6 players in Queue, got " << matchmaker.GetInfo().GetTotalPlayerCount());

        auto attemptedMatchups = matchmaker.PopMatchups();

        for (auto matchup : attemptedMatchups)
        {
            std::cout << "Attempted MMR extension Matchup: " << matchup << std::endl;
        }

        // should be unable to form a matchup
        // even though players are matching one is too high rated to be matched yet
        CHECK_MESSAGE(attemptedMatchups.size() == 0, "Expected 0 matchups, got " << attemptedMatchups.size());
        matchmaker.Update(30000); // 30 seconds passed

        auto attemptedMatchups2 = matchmaker.PopMatchups();
        for (auto matchup : attemptedMatchups2)
        {
            std::cout << "Attempted MMR extension Matchup: " << matchup << std::endl;
        }

        // MMR gap should be still too large
        CHECK_MESSAGE(attemptedMatchups2.size() == 0, "Expected 0 matchups, got " << attemptedMatchups.size());
        matchmaker.Update(60 * 60 * 1000); // 1 hour passed

        // ok now enough time should have passed...
        auto matchups = matchmaker.PopMatchups();
        for (auto matchup : matchups)
        {
            std::cout << "MMR extension Matchup: " << matchup << std::endl;
        }

        CHECK_MESSAGE(matchups.size() == 1, "Expected 1 matchup, got " << matchups.size());
    }

#endif

#if BENCHMARK
    // small benchmark
    SUBCASE("Benchmark")
    {
        arenacraft::soloq::MatchMaker matchmaker(300);
        std::cout << std::endl;

        srand(1337);
        for (int i = 0; i < 10000; i++)
        {
            arenacraft::soloq::SoloqPlayer player;
            player.playerGUID = i;
            player.classId = arenacraft::ClassId((arenacraft::ClassId)(i % 10));
            player.specIndex = random() % 3;
            player.matchMakingRating = random() % 3500;
            matchmaker.AddPlayer(player);
        }

        auto totalPlayers = matchmaker.GetInfo().GetTotalPlayerCount();

        std::cout << "There are " << totalPlayers << " players in queue" << std::endl;
        std::cout << "Melee count: " << matchmaker.GetInfo().meleeCount << std::endl;
        std::cout << "Caster count: " << matchmaker.GetInfo().casterCount << std::endl;
        std::cout << "Healer count: " << matchmaker.GetInfo().healerCount << std::endl;

        auto now = std::chrono::high_resolution_clock::now();

        uint64_t totalMatchups = 0;
        // 10 matchmaking rounds
        for (int i = 0; i < 10; i++)
        {
            auto matchups = matchmaker.PopMatchups();
            totalMatchups += matchups.size();
            matchmaker.Update(60000); // 60 seconds passed, trigger mmr extension
        }

        auto matchups = matchmaker.PopMatchups();
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - now);
        std::cout << "Matchmaking " << totalPlayers << " players took "
                  << duration.count() << "ms. "
                  << "Found " << matchups.size() << " matchups"
                  << std::endl;

        // std::cout << "Matchmaker after benchmark: " << matchmaker << std::endl;
    }

#endif
}
