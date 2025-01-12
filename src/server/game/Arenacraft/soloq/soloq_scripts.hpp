
#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <unordered_map>
#include <algorithm>

#include "Arenacraft.hpp"
#include "soloq/soloq_io.hpp"
#include "soloq/soloq_match_maker.hpp"

#include "CreatureScript.h"
#include "ScriptedGossip.h"
#include "GossipDef.h"
#include "AllBattlegroundScript.h"
#include "Player.h"

constexpr uint32 PLAYER_TAUNT_NPC_TEXT_ID = 5215;

namespace arenacraft::soloq
{

    class SoloQueueScript : public AllBattlegroundScript
    {
    private:
        arenacraft::soloq::MatchMaker matchMaker;

    public:
        SoloQueueScript() : AllBattlegroundScript("SoloQueueScript"),
                            matchMaker(250) {};

        void OnAddGroup(BattlegroundQueue *queue,
                        GroupQueueInfo * /*ginfo*/,
                        uint32 & /*index*/, Player *leader, Group * /*group*/, BattlegroundTypeId /* bgTypeId */, PvPDifficultyEntry const * /* bracketEntry */,
                        uint8 arenaType, bool isRated, bool /* isPremade */, uint32 /* arenaRating */, uint32 /* matchmakerRating */, uint32 /* arenaTeamId */, uint32 /* opponentsArenaTeamId */) override
        {
            if (arenaType != ARENA_TYPE_5v5 || !isRated)
                return;
            auto arenaTeam = arenacraft::soloq::io::GetArenaTeam(leader);
            leader->AddBattlegroundQueueId(BattlegroundQueueTypeId::BATTLEGROUND_QUEUE_5v5);
            arenacraft::soloq::SoloqPlayer soloqPlayer;
            soloqPlayer.playerGUID = leader->GetGUID().GetRawValue();
            soloqPlayer.classId = arenacraft::fns::ClassIdFromChrClassesDbcIndex(leader->getClass());
            soloqPlayer.specIndex = leader->GetMostPointsTalentTree();
            soloqPlayer.matchMakingRating = arenaTeam->GetRating();
            matchMaker.AddPlayer(soloqPlayer);
        }

        bool OnBeforeSendExitMessageArenaQueue(BattlegroundQueue * /*queue*/, GroupQueueInfo *ginfo) override
        {
            if (ginfo->IsRated && ginfo->ArenaType == ARENA_TYPE_5v5)
            {
                std::cout << "dequeue operation should happen here TODO" << std::endl;
            }
            return true;
        }

        void OnQueueUpdate(BattlegroundQueue *queue, uint32 diff, BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket_id, uint8 arenaType, bool isRated, uint32 /*arenaRatedTeamId*/) override
        {
            if (arenaType != ARENA_TYPE_5v5 || !isRated)
                return;

            matchMaker.Update(diff);
            auto matchups = matchMaker.PopMatchups();
            for (auto &matchup : matchups)
            {

                // sBattlegroundMgr->CreateBattleground()

                // matchup.ForEachPlayer([&](const arenacraft::soloq::SoloqPlayer &player) {
                //     auto p = sObjectMgr->GetPlayer(player.playerGUID);
                //     if (p)
                //     {
                //         p->GetSession()->SendArenaTeamCommandResult(ArenaTeamCommandTypes::ERR_ARENA_TEAM_CREATE_S, p->GetName(), "", ERR_ALREADY_IN_ARENA_TEAM);
                //     }
                // });
            }
        }

        bool OnQueueUpdateValidity(BattlegroundQueue * /* queue */, uint32 /*diff*/, BattlegroundTypeId /* bgTypeId */, BattlegroundBracketId /* bracket_id */, uint8 arenaType, bool isRated, uint32 /*arenaRatedTeamId*/) override
        {
            if (arenaType == ARENA_TYPE_5v5 && isRated)
                return false;
            return true;
        }
        // void OnBattlegroundDestroy(Battleground *bg) override;
        // void OnBattlegroundEndReward(Battleground *bg, Player *player, TeamId /* winnerTeamId */) override;
    };

    enum NpcSoloqAction
    {
        QUEUE,
        CREATE_TEAM,
    };

    class NpcSoloq : CreatureScript
    {

    public:
        NpcSoloq() : CreatureScript("npc_soloq") {}

        bool OnGossipHello(Player *player, Creature *creature) override
        {
            AddGossipItemFor(player, GossipOptionIcon::GOSSIP_ICON_BATTLE, "Join SoloQ", GOSSIP_SENDER_MAIN, NpcSoloqAction::QUEUE);
            AddGossipItemFor(player, GossipOptionIcon::GOSSIP_ICON_INTERACT_1, "Create SoloQ", GOSSIP_SENDER_MAIN, NpcSoloqAction::CREATE_TEAM);

            SendGossipMenuFor(player, PLAYER_TAUNT_NPC_TEXT_ID, creature->GetGUID());

            return true;
        }

        bool OnGossipSelect(Player *player, Creature *creature, uint32 /*sender*/, uint32 action) override
        {
            CloseGossipMenuFor(player);
            switch (action)
            {
            case NpcSoloqAction::QUEUE:
                return AddPlayerToSoloQueue(player);
            case NpcSoloqAction::CREATE_TEAM:
                return CreateSoloTeam(player, creature);
            }
            return false;
        }

    private:
        bool AddPlayerToSoloQueue(Player *player)
        {
            if (!arenacraft::soloq::io::HasSoloTeam(player))
            {
                ChatHandler(player->GetSession()).SendSysMessage("You need to create a team first!");
                return false;
            }

            if (!arenacraft::soloq::io::AddPlayerToSoloQueue(player))
            {
                ChatHandler(player->GetSession()).SendSysMessage("Failed to add player to solo queue!");
                return false;
            }
            return true;
        }

        bool CreateSoloTeam(Player *player, Creature *creature)
        {
            if (arenacraft::soloq::io::HasSoloTeam(player))
            {
                player->GetSession()->SendArenaTeamCommandResult(ArenaTeamCommandTypes::ERR_ARENA_TEAM_CREATE_S, player->GetName(), "", ERR_ALREADY_IN_ARENA_TEAM);
                return false;
            }

            if (!arenacraft::soloq::io::CreateSoloqTeam(player))
            {
                ChatHandler(player->GetSession()).SendSysMessage("Failed to create arena team!");
                return false;
            }

            ChatHandler(player->GetSession()).SendSysMessage("Arena team successful created!");
            return true;
        }
    };
}