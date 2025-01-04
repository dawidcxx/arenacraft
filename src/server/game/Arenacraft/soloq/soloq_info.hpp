
#pragma once

#include "Arenacraft.hpp"
#include <chrono>
#include <unordered_map>

namespace arenacraft::soloq
{
    struct SoloqInfo
    {
        uint64_t healerCount;
        uint64_t meleeCount;
        uint64_t casterCount;
        std::unordered_map<ClassId, uint64_t> classToCount;

        uint64_t GetTotalPlayerCount()
        {
            return healerCount + meleeCount + casterCount;
        }
    };
}