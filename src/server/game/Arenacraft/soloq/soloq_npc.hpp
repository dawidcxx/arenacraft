
#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <unordered_map>

#include "Arenacraft.hpp"

#include "CreatureScript.h"
#include "ScriptedGossip.h"
#include "GossipDef.h"
#include "Player.h"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "BattlegroundQueue.h"
#include "BattlegroundQueue.h"
#include "Battleground.h"

constexpr uint32 PLAYER_TAUNT_NPC_TEXT_ID = 5215;

namespace arenacraft::soloq
{
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
            BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(BattlegroundQueueTypeId::BATTLEGROUND_QUEUE_5v5);
            bgQueue.AddGroup(player, nullptr, 0, STATUS_WAIT_QUEUE, ARENA_TYPE_5v5, true, false, 0, 0, 0, 0);
            return true;
        }

        bool CreateSoloTeam(Player *player, Creature *creature)
        {
            auto priorTeam = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TYPE_5v5);
            if (priorTeam != nullptr)
            {
                player->GetSession()->SendArenaTeamCommandResult(ArenaTeamCommandTypes::ERR_ARENA_TEAM_CREATE_S, player->GetName(), "", ERR_ALREADY_IN_ARENA_TEAM);
                return false;
            }

            ArenaTeam *arenaTeam = new ArenaTeam();
            std::ostringstream teamNameBuilder;
            teamNameBuilder << player->GetName();
            teamNameBuilder << "'s 3v3 SoloQ Team";

            if (arenaTeam->Create(
                    player->GetGUID(),
                    ARENA_TYPE_5v5,
                    teamNameBuilder.str(),
                    0,
                    0,
                    0,
                    0,
                    0))
            {
                sArenaTeamMgr->AddArenaTeam(arenaTeam);
                ChatHandler(player->GetSession()).SendSysMessage("Arena team successful created!");
                return true;
            }
            else
            {
                delete arenaTeam;
                ChatHandler(player->GetSession()).SendSysMessage("Failed to create arena team!");
                return false;
            }
        }
    };
}