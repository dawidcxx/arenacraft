#pragma once

#include <list>

#include "soloq/soloq_player.hpp"

namespace arenacraft::soloq
{
    using PlayerList = std::list<SoloqPlayer>;

    struct QueueList
    {
        PlayerList healers;
        PlayerList casters;
        PlayerList melees;

        void ForEachPlayer(std::function<void(SoloqPlayer &)> func)
        {
            for (auto &healer : healers)
            {
                func(healer);
            }
            for (auto &caster : casters)
            {
                func(caster);
            }
            for (auto &melee : melees)
            {
                func(melee);
            }
        }
        
    };

    std::ostream &operator<<(std::ostream &os, const QueueList &queueList)
    {
        os << "QueueList {";
        os << "healers: [";
        for (auto healer : queueList.healers)
        {
            os << healer << ", ";
        }
        os << "] ";
        os << "casters: [";
        for (auto caster : queueList.casters)
        {
            os << caster << ", ";
        }
        os << "] ";
        os << "melees: [";
        for (auto melee : queueList.melees)
        {
            os << melee << ", ";
        }
        os << "] ";
        os << "}";
        return os;
    };
    
}