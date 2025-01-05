
#pragma once

#include "Arenacraft.hpp"
#include "BattlegroundQueue.h"
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "DBCStructure.h"
#include "soloq/soloq_constants.hpp"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"

#include <iostream>

namespace arenacraft::soloq::io
{

    bool CreateSoloqTeam(Player *player)
    {
        ArenaTeam *arenaTeam = new ArenaTeam();
        std::ostringstream teamNameBuilder;
        teamNameBuilder << player->GetName();
        teamNameBuilder << "'s 3v3 SoloQ Team";

        if (arenaTeam->Create(
                player->GetGUID(), ARENA_TYPE_5v5, teamNameBuilder.str(),
                0, 0, 0, 0, 0 // black bordered team banner
                ))
        {
            sArenaTeamMgr->AddArenaTeam(arenaTeam);
            return true;
        }
        else
        {
            delete arenaTeam;
            return false;
        }
    }

    bool AddPlayerToSoloQueue(Player *player)
    {
        BattlegroundQueue &bgQueue = sBattlegroundMgr->GetBattlegroundQueue(BattlegroundQueueTypeId::BATTLEGROUND_QUEUE_5v5);
        PvPDifficultyEntry pvpDifficultyEntry(0, soloq::constants::BRACKET_ID, 80, 80, 0);

        GroupQueueInfo *ginfo = bgQueue.AddGroup(
            player,
            nullptr, // group
            BattlegroundTypeId::BATTLEGROUND_AA,
            &pvpDifficultyEntry,
            ARENA_TYPE_5v5,
            true, // isRated
            false,
            0, 0, 0, 0);

        if (!ginfo)
        {
            return false;
        }

        WorldPacket data(SMSG_BATTLEFIELD_STATUS, 34);
        data << uint32(0);  // QueueSlot
        data << uint8(3);   // ArenaType (3v3)
        data << uint8(0xE); // means it's arena

        data << uint32(BattlegroundTypeId::BATTLEGROUND_AA);
        data << uint16(0x1F90); // unknown

        data << uint8(79); // Min Level
        data << uint8(81); // Max Level

        data << uint32(0); // InstanceID

        data << uint8(1);                                      // isRated
        data << uint32(BattlegroundStatus::STATUS_WAIT_QUEUE); // STATUS_WAIT_QUEUE
        data << uint32(0);                                     // average wait time
        data << uint32(0);                                     // time in queue

        player->GetSession()->SendPacket(&data);

        return true;
    }


    bool HasSoloTeam(Player *player)
    {
        auto team = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TYPE_5v5);
        return team != nullptr;
    }
}