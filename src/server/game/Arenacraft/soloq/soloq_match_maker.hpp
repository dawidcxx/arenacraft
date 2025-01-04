#pragma once

#include "Arenacraft.hpp"
#include "soloq/soloq_player.hpp"
#include "soloq/soloq_matchup.hpp"
#include "soloq/soloq_queue_list.hpp"
#include "soloq/soloq_match_maker.hpp"
#include "soloq/soloq_info.hpp"

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <cassert>
#include <queue>
#include <optional>
#include <list>
#include <algorithm>
#include <chrono>

#define SOLOQ_MIN_RATING (uint32_t)0
#define SOLOQ_MAX_RATING (uint32_t)3500

namespace arenacraft::soloq
{
    class MatchMaker
    {
    private:
        uint32_t m_mmrQuant;
        std::unordered_map<uint32_t, QueueList> m_mmrToQueueList;

    public:
        static MatchMaker &Instance()
        {
            static MatchMaker instance(100);
            return instance;
        }

        MatchMaker(uint32_t mmrQuant)
        {
            m_mmrQuant = mmrQuant;
            uint32_t brackets = SOLOQ_MAX_RATING / m_mmrQuant;
            for (uint32_t i = 0; i < brackets; i++)
            {
                m_mmrToQueueList.insert(std::make_pair(mmrQuant * i, QueueList()));
            }
        }

        void AddPlayer(SoloqPlayer player)
        {
            auto bracket = GetPlayerBracket(player);
            auto &queueList = m_mmrToQueueList[bracket];
            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);
            AddToQueueList(player, queueList);
        }

        void RemovePlayer(SoloqPlayer player)
        {
            auto bracket = GetPlayerBracket(player);
            auto &queueList = m_mmrToQueueList[bracket];
            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);
            RemoveFromQueueList(player, queueList);
        }

        std::vector<QueuePopMatchup> PopMatchups()
        {
            std::vector<QueuePopMatchup> matchups;

            for (auto &[mmrBracket, queueList] : m_mmrToQueueList)
            {
                if (QueueHasEnoughPlayers(queueList))
                {
                    QueuePopMatchup matchup;

                    matchup.Append(queueList.melees);
                    matchup.Append(queueList.casters);
                    matchup.Append(queueList.healers);

                    if (matchup.IsComplete())
                    {
                        matchups.push_back(matchup);
                        matchup.ForEachPlayer([&](SoloqPlayer &player) {
                            RemoveFromQueueList(player, queueList);
                        });
                    }
                }
            }
            return matchups;
        };


        SoloqInfo GetInfo()
        {
            SoloqInfo info;
            info.healerCount = 0;
            info.meleeCount = 0;
            info.casterCount = 0;

            for (auto &[mmrBracket, queueList] : m_mmrToQueueList)
            {
                info.healerCount += queueList.healers.size();
                info.meleeCount += queueList.melees.size();
                info.casterCount += queueList.casters.size();
            }

            return info;
        };


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

        void AddToQueueList(SoloqPlayer player, QueueList &queueList)
        {
            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);
            switch (role)
            {
            case PlayerRole::ROLE_HEALER:
            {
                queueList.healers.push_back(player);
                break;
            }
            case PlayerRole::ROLE_CASTER:
            {
                queueList.casters.push_back(player);
                break;
            }
            case PlayerRole::ROLE_MELEE:
            {
                queueList.melees.push_back(player);
                break;
            }
            default:
                throw new std::out_of_range("Invalid role");
            }
        }

        void RemoveFromQueueList(SoloqPlayer player, QueueList &queueList)
        {
            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);
            switch (role)
            {
            case PlayerRole::ROLE_HEALER:
            {
                queueList.healers.remove(player);
                break;
            }
            case PlayerRole::ROLE_CASTER:
            {
                queueList.casters.remove(player);
                break;
            }
            case PlayerRole::ROLE_MELEE:
            {
                queueList.melees.remove(player);
                break;
            }
            default:
                throw new std::out_of_range("Invalid role");
            }
        }
    };

} // namespace arenacraft
