
#pragma once

#include "Arenacraft.hpp"
#include <chrono>
#include <stdint.h>

namespace arenacraft::soloq
{
    struct SoloqPlayer
    {
        uint64_t playerGUID;
        ClassId classId;
        uint32_t matchMakingRating;
        uint32_t specIndex;
    };

    // Player Eq
    bool operator==(const SoloqPlayer &lhs, const SoloqPlayer &rhs)
    {
        return lhs.playerGUID == rhs.playerGUID;
    }

    // Player Hash
    struct SoloqPlayerHash
    {
        std::size_t operator()(const SoloqPlayer &player) const
        {
            return std::hash<uint64_t>{}(player.playerGUID);
        }
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