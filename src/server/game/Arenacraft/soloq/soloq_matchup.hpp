#pragma once

#include <soloq/soloq_player.hpp>
#include <soloq/soloq_queue_list.hpp>
#include <optional>
#include <unordered_set>

namespace arenacraft::soloq
{
    struct QueuePopMatchup
    {
        std::vector<SoloqPlayer> team1;
        std::vector<SoloqPlayer> team2;

        // avoid class stacking
        std::unordered_set<ClassId> _team1ClassIds;
        std::unordered_set<ClassId> _team2ClassIds;

        // avoid player stacking
        std::unordered_set<uint64_t> _playersTakenGuids;

        bool IsComplete()
        {
            return team1.size() == 3 && team2.size() == 3;
        };

        void ForEachPlayer(std::function<void(SoloqPlayer &)> func)
        {
            for (auto &player : team1)
            {
                func(player);
            }
            for (auto &player : team2)
            {
                func(player);
            }
        }

        void Append(soloq::PlayerList &playerPool)
        {
            for (auto player : playerPool)
            {
                if (_playersTakenGuids.contains(player.playerGUID))
                {
                    continue;
                }
                if (_team1ClassIds.contains(player.classId))
                {
                    continue;
                }
                _playersTakenGuids.insert(player.playerGUID);
                _team1ClassIds.insert(player.classId);
                team1.push_back(player);
                break;
            }

            for (auto player : playerPool)
            {
                if (_playersTakenGuids.contains(player.playerGUID))
                {
                    continue;
                }
                if (_team2ClassIds.contains(player.classId))
                {
                    continue;
                }
                _playersTakenGuids.insert(player.playerGUID);
                _team2ClassIds.insert(player.classId);
                team2.push_back(player);
                break;
            }

        }
    };

    std::ostream &
    operator<<(std::ostream &os, const QueuePopMatchup &matchup)
    {
        os << "QueuePopMatchup {";
        os << "team1: [";
        for (auto player : matchup.team1)
        {
            os << player << ", ";
        }
        os << "] ";
        os << "team2: [";
        for (auto player : matchup.team2)
        {
            os << player << ", ";
        }
        os << "] ";
        os << "}";
        return os;
    };


};