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
#include <ctime>

#define SOLOQ_MIN_RATING (uint32_t)0
#define SOLOQ_MAX_RATING (uint32_t)3500

namespace arenacraft::soloq
{
    class MatchMaker
    {
    private:
        uint32_t m_mmrQuant;
        std::unordered_map<uint32_t, QueueList> m_mmrToQueueList;
        std::unordered_map<SoloqPlayer, double, SoloqPlayerHash> m_playerWaitTimes;
        double m_elapsed_seconds = 0;

        // quick lookup to players by class
        std::unordered_map<ClassId, std::unordered_set<SoloqPlayer, SoloqPlayerHash>> m_classToPlayers;
        // quick lookup to players by role
        std::unordered_map<PlayerRole, std::unordered_set<SoloqPlayer, SoloqPlayerHash>> m_roleToPlayers;

    public:
        static MatchMaker &Instance()
        {
            static MatchMaker instance(100);
            return instance;
        };

        MatchMaker(uint32_t mmrQuant)
        {
            m_mmrQuant = mmrQuant;
            uint32_t brackets = SOLOQ_MAX_RATING / m_mmrQuant;
            for (uint32_t i = 0; i < brackets; i++)
            {
                m_mmrToQueueList.insert(std::make_pair(mmrQuant * i, QueueList()));
            }
        };

        std::vector<QueuePopMatchup> PopMatchups()
        {
            std::vector<QueuePopMatchup> matchups;

            for (auto &[mmrBracket, queueList] : m_mmrToQueueList)
            {
                if (QueueHasEnoughPlayers(queueList))
                {
                    // std::cout << "Bracket [ " << mmrBracket << " ] => Queue List: " << queueList << std::endl;    

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

        void Update(uint32_t diffInMs)
        {
            double prior = m_elapsed_seconds;
            m_elapsed_seconds += diffInMs / 1000.0;

            // Extend MMR brackets for players
            for (auto &[player, _] : m_playerWaitTimes)
            {
                auto bracketsPrior = CalculatePlayerBrackets(player, prior);
                auto bracketsAfter = CalculatePlayerBrackets(player, m_elapsed_seconds);
                auto bracketsAdded = std::vector<uint32_t>();

                std::set_difference(bracketsAfter.begin(), bracketsAfter.end(),
                                    bracketsPrior.begin(), bracketsPrior.end(),
                                    std::inserter(bracketsAdded, bracketsAdded.begin()));

                for (auto bracket : bracketsAdded)
                {
                    auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);
                    auto &queueList = m_mmrToQueueList[bracket];
                    AddToQueueList(player, role, queueList);
                }
            }
        }

        void AddPlayer(SoloqPlayer player)
        {
            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);

            m_playerWaitTimes[player] = m_elapsed_seconds;
            m_classToPlayers[player.classId].insert(player);
            m_roleToPlayers[role].insert(player);

            for (uint32_t bracket : CalculatePlayerBrackets(player, m_elapsed_seconds))
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
            m_playerWaitTimes.erase(player);

            for (uint32_t bracket : CalculatePlayerBrackets(player, m_elapsed_seconds))
            {
                auto &queueList = m_mmrToQueueList[bracket];
                RemoveFromQueueList(player, role, queueList);
            }
        }

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

        std::vector<uint32_t> CalculatePlayerBrackets(const SoloqPlayer &player, double elapsed_seconds)
        {
            // for example    (1734 / 100) * 100 = 1700
            uint32_t bracketBase = (player.matchMakingRating / m_mmrQuant) * m_mmrQuant;

            // every 30 seconds in queue players grants one more bracket extension
            // up to 10 times
            uint8_t bracketExtensions = std::min(static_cast<uint8_t>(10), static_cast<uint8_t>((elapsed_seconds - m_playerWaitTimes[player]) / 30));

            // output list
            // example [1700, 1600, 1500]
            std::vector<uint32_t>
                brackets(std::max((uint8_t)1, bracketExtensions));

            uint32_t i = 0;
            do
            {
                // for example     1700    -    0  * 100 = 1700
                //                 1700    -    1  * 100 = 1600
                uint32_t bracket = bracketBase - i * m_mmrQuant;
                // make sure it's within <SOLOQ_MIN_RATING, SOLOQ_MAX_RATING> range
                uint32_t bracketNormalized = std::min(std::max(bracket, SOLOQ_MIN_RATING), SOLOQ_MAX_RATING);
                brackets[i] = bracketNormalized;
                i += 1;
            } while (i < bracketExtensions);

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
