
#pragma once

#include "Arenacraft.hpp"

namespace arenacraft::soloq
{
    struct SoloqPlayer
    {
        uint64_t playerGUID;
        ClassId classId;
        uint32_t matchMakingRating;
        uint32_t specIndex;
    };
   
    std::ostream &operator<<(std::ostream &os, const SoloqPlayer &player)
    {
        os << "Player {";
        os << "GUID: " << player.playerGUID << " ";
        os << "Class: " << fns::GetClassName(player.classId) << " ";
        os << "MMR: " << player.matchMakingRating << " ";
        os << "Spec: " << player.specIndex << " ";
        os << "}";
        return os;
    }
}