
#pragma once

#include "Arenacraft.hpp"
#include <chrono>

namespace arenacraft::soloq
{
    struct SoloqInfo
    {
        uint64_t healerCount;
        uint64_t meleeCount;
        uint64_t casterCount;

        // TODO: Implement this
        std::chrono::minutes averageWaitTime;

        uint64_t GetTotalPlayerCount()
        {
            return healerCount + meleeCount + casterCount;
        }
    };
}