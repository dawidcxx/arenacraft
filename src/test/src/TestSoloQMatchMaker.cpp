
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "soloq/soloq_match_maker.hpp"

// Example test cases
TEST_CASE("MatchMaker scenario test suite")
{

    auto matchmaker = arenacraft::MatchMaker::Instance();

    SUBCASE("the most basic example should work")
    {
        matchmaker.Clear();
        arenacraft::SoloqPlayer player1{1, arenacraft::CLASS_WARRIOR, 1000, 0};
        arenacraft::SoloqPlayer player2{2, arenacraft::CLASS_PALADIN, 1000, 0};
        arenacraft::SoloqPlayer player3{3, arenacraft::CLASS_WARLOCK, 1000, 0};

        arenacraft::SoloqPlayer player4{4, arenacraft::CLASS_DEATH_KNIGHT, 1000, 2};
        arenacraft::SoloqPlayer player5{5, arenacraft::CLASS_PRIEST, 1000, 1};
        arenacraft::SoloqPlayer player6{6, arenacraft::CLASS_SHAMAN, 1000, 0};

        matchmaker.AddPlayer(player1);
        matchmaker.AddPlayer(player2);
        matchmaker.AddPlayer(player3);
        matchmaker.AddPlayer(player4);
        matchmaker.AddPlayer(player5);
        matchmaker.AddPlayer(player6);

        auto queuePopResult = matchmaker.TryPopQueue();

        CHECK(queuePopResult.has_value());
        auto queuePop = queuePopResult.value();

        std::cout << std::endl << queuePop << std::endl;
    }
}
