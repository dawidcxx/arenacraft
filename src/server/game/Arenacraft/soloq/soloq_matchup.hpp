#pragma once

#include <soloq/soloq_player.hpp>

namespace arenacraft::soloq
{
    struct QueuePopMatchup
    {
        std::vector<SoloqPlayer *> team1;
        std::vector<SoloqPlayer *> team2;
    };

    // overload operator for queuepopmatchup
    std::ostream &operator<<(std::ostream &os, const QueuePopMatchup &matchup)
    {
        os << "QueuePopMatchup {";
        os << "team1: [";
        for (auto player : matchup.team1)
        {
            os << *player << ", ";
        }
        os << "] ";
        os << "team2: [";
        for (auto player : matchup.team2)
        {
            os << *player << ", ";
        }
        os << "] ";
        os << "}";
        return os;
    }
}