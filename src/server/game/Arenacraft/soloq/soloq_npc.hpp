
#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <unordered_map>

#include "Arenacraft.hpp"
#include "soloq/soloq_io.hpp"

#include "CreatureScript.h"
#include "ScriptedGossip.h"
#include "GossipDef.h"
#include "Player.h"

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