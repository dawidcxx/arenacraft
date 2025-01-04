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

        // quick lookup to players by class
        std::unordered_map<ClassId, std::unordered_set<SoloqPlayer, SoloqPlayerHash>> m_classToPlayers;
        // quick lookup to players by role
        std::unordered_map<PlayerRole, std::unordered_set<SoloqPlayer, SoloqPlayerHash>> m_roleToPlayers;

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
            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);

            m_classToPlayers[player.classId].insert(player);
            m_roleToPlayers[role].insert(player);

            for (uint32_t bracket : GetPlayerBracket(player))
            {
                auto &queueList = m_mmrToQueueList[bracket];
                AddToQueueList(player, role, queueList);
            }
        }

        void RemovePlayer(SoloqPlayer player)
        {
            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);

            m_classToPlayers[player.classId].erase(player);
            m_roleToPlayers[role].erase(player);

            for (uint32_t bracket : GetPlayerBracket(player))
            {
                auto &queueList = m_mmrToQueueList[bracket];
                RemoveFromQueueList(player, role, queueList);
            }
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
                        matchup.ForEachPlayer([&](SoloqPlayer &player)
                                              { RemovePlayer(player); });
                    }
                }
            }
            return matchups;
        };

        SoloqInfo GetInfo()
        {
            SoloqInfo info;
            info.healerCount = m_roleToPlayers[PlayerRole::ROLE_HEALER].size();
            info.meleeCount = m_roleToPlayers[PlayerRole::ROLE_MELEE].size();
            info.casterCount = m_roleToPlayers[PlayerRole::ROLE_CASTER].size();
            for (auto &[classId, players] : m_classToPlayers)
            {
                info.classToCount[classId] = players.size();
            }
            return info;
        };

    private:
        bool QueueHasEnoughPlayers(QueueList &queueList)
        {
            return queueList.healers.size() >= 2 && queueList.casters.size() >= 2 && queueList.melees.size() >= 2;
        }

        std::vector<uint32_t> GetPlayerBracket(SoloqPlayer &player)
        {
            // for example    (1734 / 100) * 100 = 1700
            uint32_t bracketBase = (player.matchMakingRating / m_mmrQuant) * m_mmrQuant;

            std::vector<uint32_t> brackets(std::max((uint8_t)1, player.waitedIterations));

            uint32_t i = 0;
            do
            {
                // for example    1700 - 0 * 100 = 1700
                //                1700 - 1 * 100 = 1600
                uint32_t bracket = bracketBase - i * m_mmrQuant;
                // make sure it's within <SOLOQ_MIN_RATING, SOLOQ_MAX_RATING> range
                uint32_t bracketNormalized = std::min(std::max(bracket, SOLOQ_MIN_RATING), SOLOQ_MAX_RATING);
                brackets[i] = bracketNormalized;
            } while (i < player.waitedIterations);

            return brackets;
        }

        void AddToQueueList(SoloqPlayer player, PlayerRole role, QueueList &queueList)
        {
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

        void RemoveFromQueueList(SoloqPlayer player, PlayerRole role, QueueList &queueList)
        {
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
