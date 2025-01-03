#pragma once

#include "Arenacraft.hpp"
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <cassert>
#include <optional>

namespace arenacraft
{

    struct SoloqPlayer
    {
        uint64_t playerGUID;
        ClassId classId;
        uint32_t matchMakingRating;
        uint32_t specIndex;
    };

    struct QueuePopResult
    {
        std::vector<SoloqPlayer *> team1;
        std::vector<SoloqPlayer *> team2;
    };

    struct Team
    {
        std::unordered_set<ClassId> classes;
        SoloqPlayer *melee = nullptr;
        SoloqPlayer *caster = nullptr;
        SoloqPlayer *healer = nullptr;

        bool Assembled()
        {
            return melee != nullptr && caster != nullptr && healer != nullptr;
        }

        bool NeedsMelee()
        {
            return melee == nullptr;
        }

        bool NeedsCaster()
        {
            return caster == nullptr;
        }

        bool NeedsHealer()
        {
            return healer == nullptr;
        }

        bool TryAppend(SoloqPlayer *player)
        {
            // Avoid class stacking
            if (classes.contains(player->classId))
            {
                return false;
            }

            auto classId = player->classId;
            auto specIndex = player->specIndex;

            switch (fns::GetRoleForClassAndSpec(classId, specIndex))
            {
            case PlayerRole::ROLE_MELEE:
                if (NeedsMelee())
                {
                    melee = player;
                    return true;
                }
                return false;
            case PlayerRole::ROLE_CASTER:
                if (NeedsCaster())
                {
                    caster = player;
                    return true;
                }
                return false;
            case PlayerRole::ROLE_HEALER:
                if (NeedsHealer())
                {
                    healer = player;
                    return true;
                }
                return false;
            default:
                assert(false);
            }
        }

        uint32_t GetAvgMmr()
        {
            return (melee->matchMakingRating + caster->matchMakingRating + healer->matchMakingRating) / 3;
        }

        bool IsWithinRange(Team &team)
        {
            auto avgMmr = GetAvgMmr();
            auto teamAvgMmr = team.GetAvgMmr();
            return avgMmr >= teamAvgMmr - 350 && avgMmr <= teamAvgMmr + 350;
        }
    };

    class MatchMaker
    {
    private:
        using MMRLookupMap = std::multimap<uint32_t, SoloqPlayer *>;
        using RoleLookupMap = std::unordered_multimap<PlayerRole, SoloqPlayer *>;

        MMRLookupMap playersByMmr;
        RoleLookupMap playersByRoles;

        // back references to enable quick removal from queue
        std::unordered_map<SoloqPlayer *, MMRLookupMap::iterator> playersByMmrBackRef;
        std::unordered_map<SoloqPlayer *, RoleLookupMap::iterator> playersByRolesBackRef;

    public:
        static MatchMaker &Instance()
        {
            static MatchMaker instance;
            return instance;
        }

        MatchMaker()
        {
            playersByMmr = MMRLookupMap();
            playersByRoles = RoleLookupMap();
            playersByMmrBackRef = std::unordered_map<SoloqPlayer *, MMRLookupMap::iterator>();
            playersByRolesBackRef = std::unordered_map<SoloqPlayer *, RoleLookupMap::iterator>();
        }

        void AddPlayer(SoloqPlayer &player)
        {

            auto mmrEntry = playersByMmr.insert(std::make_pair(player.matchMakingRating, &player));
            playersByMmrBackRef.insert(std::make_pair(&player, mmrEntry));

            auto role = fns::GetRoleForClassAndSpec(player.classId, player.specIndex);
            auto roleEntry = playersByRoles.insert(std::make_pair(role, &player));
            playersByRolesBackRef.insert(std::make_pair(&player, roleEntry));
        }

        void RemovePlayer(SoloqPlayer &player)
        {
            playersByMmr.erase(playersByMmrBackRef[&player]);
            playersByRoles.erase(playersByRolesBackRef[&player]);
            playersByMmrBackRef.erase(&player);
            playersByRolesBackRef.erase(&player);
        }

        std::optional<QueuePopResult> TryPopQueue()
        {
            if (CanSkipQueueProcessing())
            {
                return std::nullopt;
            }

            std::unordered_set<SoloqPlayer *> playersTaken;
            std::map<uint32_t, Team> teams;

            for (auto playersByMmmrIt = playersByMmr.begin(); playersByMmmrIt != playersByMmr.end();)
            {
                while (playersByMmmrIt != playersByMmr.end())
                {
                    auto leaderPlayer = playersByMmmrIt->second;

                    if (auto teamOp = TryAssembleTeam(leaderPlayer, playersTaken))
                    {
                        auto &team = *teamOp;
                        playersTaken.insert(team.melee);
                        playersTaken.insert(team.caster);
                        playersTaken.insert(team.healer);
                        teams.insert(std::make_pair(team.GetAvgMmr(), team));
                    }
                    else
                    {
                        ++playersByMmmrIt;
                    }
                }
            }

            for (auto teamIt = teams.begin(); teamIt != teams.end();)
            {
                auto team = teamIt->second;
                auto mmr = team.GetAvgMmr();
                auto teamsLower = teams.lower_bound(mmr);
                auto teamsUpper = teams.upper_bound(mmr + 350);

                for (auto it = teamsLower; it != teamsUpper; ++it)
                {
                    if (team.IsWithinRange(it->second))
                    {
                        QueuePopResult result;
                        for (auto player : {team.melee, team.caster, team.healer})
                        {
                            result.team1.push_back(player);
                        }
                        for (auto player : {it->second.melee, it->second.caster, it->second.healer})
                        {
                            result.team2.push_back(player);
                        }
                        return result;
                    }
                }

                teamIt++;
            }

            return std::nullopt;
        }

    private:
        std::optional<Team> TryAssembleTeam(SoloqPlayer *initPlayer,
                                            std::unordered_set<SoloqPlayer *> allPlayersTaken)
        {
            uint32_t mmr = initPlayer->matchMakingRating;

            std::unordered_set<SoloqPlayer *> playersTaken = allPlayersTaken;
            auto lower_mmr_it = playersByMmr.lower_bound(mmr - 350);
            auto upper_mmr_it = playersByMmr.upper_bound(mmr + 350);

            Team team;
            assert(team.TryAppend(initPlayer));
            playersTaken.insert(initPlayer);

            for (auto it = lower_mmr_it; it != upper_mmr_it; ++it)
            {
                if (team.Assembled())
                {
                    break;
                }
                auto candidate = it->second;
                if (!playersTaken.contains(candidate) && team.TryAppend(candidate))
                {
                    playersTaken.insert(candidate);
                }
            }

            if (team.Assembled())
            {
                return team;
            }
            else
            {
                return std::nullopt;
            }
        }

        // No point to even start processing the queue
        // if these conditions aren't met
        bool CanSkipQueueProcessing()
        {
            if (playersByMmr.size() < 6)
            {
                return true;
            }
            auto healersCount = playersByRoles.count(PlayerRole::ROLE_HEALER);
            if (healersCount < 2)
            {
                return true;
            }
            auto meleeCount = playersByRoles.count(PlayerRole::ROLE_MELEE);
            if (meleeCount < 2)
            {
                return true;
            }
            auto castersCount = playersByRoles.count(PlayerRole::ROLE_CASTER);
            if (castersCount < 2)
            {
                return true;
            }
            return false;
        }
    };

} // namespace arenacraft
