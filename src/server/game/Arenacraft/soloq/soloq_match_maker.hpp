#pragma once

#include "Arenacraft.hpp"
#include "soloq/soloq_player.hpp"
#include "soloq/soloq_matchup.hpp"
#include "soloq/soloq_queue_list.hpp"
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <cassert>
#include <queue>
#include <optional>
#include <list>
#include <algorithm>

#define SOLOQ_MIN_RATING (uint32_t) 0
#define SOLOQ_MAX_RATING (uint32_t) 3500

namespace arenacraft::soloq
{
    class MatchMaker
    {
    private:
        uint32_t m_mmrQuant;
        std::map<uint32_t, QueueList> m_mmrToQueueList;

    public:
        static MatchMaker &Instance()
        {
            static MatchMaker instance(100);
            return instance;
        }

        MatchMaker(uint32_t mmrQuant)
        {
            uint32_t brackets = SOLOQ_MAX_RATING / mmrQuant;
            m_mmrToQueueList = std::map<uint32_t, QueueList>();
            for (uint32_t i = 0; i < brackets; i++)
            {
                m_mmrToQueueList.insert(std::make_pair(mmrQuant * i, QueueList()));
            }
            m_mmrQuant = mmrQuant;
        }

        void AddPlayer(SoloqPlayer &player)
        {
            auto bracket = GetPlayerBracket(player);
            auto &queueList = m_mmrToQueueList[bracket];
            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);
            AddToQueueList(player, queueList);
        }

        void RemovePlayer(SoloqPlayer &player)
        {
            auto bracket = GetPlayerBracket(player);
            auto &queueList = m_mmrToQueueList[bracket];
            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);
            RemoveFromQueueList(player, queueList);
        }

        std::vector<QueuePopMatchup> GetMatchups()
        {
            std::vector<QueuePopMatchup> matchups;
            for (auto &[mmrBracket, queueList] : m_mmrToQueueList)
            {
                std::cout << "Bracket: " << mmrBracket << std::endl;
                std::cout << "QueueList: " << queueList << std::endl;
                if (QueueHasEnoughPlayers(queueList))
                {
                    std::cout << "Queue has enough players" << std::endl;
                    QueuePopMatchup matchup;
                    matchup.team1.push_back(queueList.healers.front());
                    RemovePlayer(*queueList.healers.front());

                    matchup.team1.push_back(queueList.casters.front());
                    RemovePlayer(*queueList.casters.front());

                    matchup.team1.push_back(queueList.melees.front());
                    RemovePlayer(*queueList.melees.front());

                    matchup.team2.push_back(queueList.healers.front());
                    RemovePlayer(*queueList.healers.front());

                    matchup.team2.push_back(queueList.casters.front());
                    RemovePlayer(*queueList.casters.front());

                    matchup.team2.push_back(queueList.melees.front());
                    RemovePlayer(*queueList.melees.front());

                    matchups.push_back(matchup);
                }
            }
            return matchups;
        }


    private:

        bool QueueHasEnoughPlayers(QueueList &queueList)
        {
            return queueList.healers.size() >= 2 && queueList.casters.size() >= 2 && queueList.melees.size() >= 2;
        }

        uint32_t GetPlayerBracket(SoloqPlayer &player)
        {
            // for example    (1734 / 100) * 100 = 1700
            uint32_t mmrBracket = (player.matchMakingRating / m_mmrQuant) * m_mmrQuant;
            // make sure it's within <SOLOQ_MIN_RATING, SOLOQ_MAX_RATING> range
            return std::min(std::max(mmrBracket, SOLOQ_MIN_RATING), SOLOQ_MAX_RATING);
        }

        void AddToQueueList(SoloqPlayer &player, QueueList &queueList)
        {
            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);
            switch (role)
            {
            case PlayerRole::ROLE_HEALER:
            {
                queueList.healers.push_back(&player);
                break;
            }
            case PlayerRole::ROLE_CASTER:
            {
                queueList.casters.push_back(&player);
                break;
            }
            case PlayerRole::ROLE_MELEE:
            {
                queueList.melees.push_back(&player);
                break;
            }
            default:
                throw new std::out_of_range("Invalid role");
            }
        }

        void RemoveFromQueueList(SoloqPlayer &player, QueueList &queueList)
        {
            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);
            switch (role)
            {
            case PlayerRole::ROLE_HEALER:
            {
                queueList.healers.remove(&player);
                break;
            }
            case PlayerRole::ROLE_CASTER:
            {
                queueList.casters.remove(&player);
                break;
            }
            case PlayerRole::ROLE_MELEE:
            {
                queueList.melees.remove(&player);
                break;
            }
            default:
                throw new std::out_of_range("Invalid role");
            }
        }
    };

   

} // namespace arenacraft
