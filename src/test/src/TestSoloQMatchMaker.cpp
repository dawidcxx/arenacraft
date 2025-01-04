
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "soloq/soloq_match_maker.hpp"

TEST_CASE("MatchMaker scenario test suite")
{
    SUBCASE("the most basic example should work")
    {
        arenacraft::soloq::MatchMaker matchmaker(100);

        arenacraft::soloq::SoloqPlayer player1{1, arenacraft::CLASS_WARRIOR, 1001, 0};
        arenacraft::soloq::SoloqPlayer player2{2, arenacraft::CLASS_PALADIN, 1020, 0};
        arenacraft::soloq::SoloqPlayer player3{3, arenacraft::CLASS_WARLOCK, 1080, 0};

        arenacraft::soloq::SoloqPlayer player4{4, arenacraft::CLASS_DEATH_KNIGHT, 1010, 2};
        arenacraft::soloq::SoloqPlayer player5{5, arenacraft::CLASS_PRIEST, 1090, 1};
        arenacraft::soloq::SoloqPlayer player6{6, arenacraft::CLASS_SHAMAN, 1002, 0};

        matchmaker.AddPlayer(player1);
        matchmaker.AddPlayer(player2);
        matchmaker.AddPlayer(player3);
        matchmaker.AddPlayer(player4);
        matchmaker.AddPlayer(player5);
        matchmaker.AddPlayer(player6);

        auto matchups = matchmaker.GetMatchups();
        std::cout << "Found matchup count: " << matchups.size() << std::endl;

        for (auto &matchup : matchups)
        {
            std::cout << matchup << std::endl;
        }
    }
}
