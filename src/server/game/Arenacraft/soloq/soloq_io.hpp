
#pragma once


#include "Arenacraft.hpp"
#include "soloq/soloq_matchup.hpp"

#include "BattlegroundQueue.h"
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "DBCStructure.h"
#include "soloq/soloq_constants.hpp"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"

#include <iostream>

namespace arenacraft::soloq::io
{
    void CreateGame(arenacraft::soloq::QueuePopMatchup matchup)
    {
        auto arenaInstance = sBattlegroundMgr->CreateNewBattleground(
            BATTLEGROUND_AA,
            constants::SOLOQ_DIFFICULTY_ENTRY,
            ArenaType::ARENA_TYPE_5v5,
            true);
        ASSERT(arenaInstance);

        matchup.ForEachPlayer([&](const arenacraft::soloq::SoloqPlayer &player) {
        
        });
        
    }

    bool CreateSoloqTeam(Player *player)
    {
        ArenaTeam *arenaTeam = new ArenaTeam();
        std::ostringstream teamNameBuilder;
        teamNameBuilder << player->GetName();
        teamNameBuilder << "'s SoloQ";

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

        GroupQueueInfo *ginfo = bgQueue.AddGroup(
            player,
            nullptr, // no group
            BattlegroundTypeId::BATTLEGROUND_AA,
            constants::SOLOQ_DIFFICULTY_ENTRY,
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

    ArenaTeam *GetArenaTeam(Player *player)
    {
        return sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TYPE_5v5);
    }

    bool HasSoloTeam(Player *player)
    {
        return GetArenaTeam(player) != nullptr;
    }

}