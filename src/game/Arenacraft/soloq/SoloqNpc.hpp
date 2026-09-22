#pragma once

#include "AllBattlegroundScript.h"
#include "AllCreatureScript.h"
#include "Creature.h"
#include "GossipDef.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "WorldScript.h"

namespace arenacraft::soloq
{
// Temporary front-end for the soloq queue: entry 20810 ("Mehrdad") is hijacked
// with a gossip menu that drives SoloqService. No arena is created yet; a found
// match is logged and the players are notified (see SoloqDriver).
class SoloqNpc : public AllCreatureScript
{
public:
  static constexpr uint32 Entry        = 20810;
  static constexpr uint32 GossipMenuId = 9100001;
  // Runtime npc_text id for the queue stats shown as the gossip page text
  // (see CanCreatureGossipHello). Never persisted to the DB.
  static constexpr uint32 StatsTextId = 9100002;

  static constexpr uint32 ActionJoin   = 9100010;
  static constexpr uint32 ActionLeave  = 9100011;
  static constexpr uint32 ActionCreate = 9100012;
  static constexpr uint32 ActionDelete = 9100013;

  SoloqNpc() : AllCreatureScript("arenacraft::soloq::SoloqNpc") {}

  void OnCreatureAddWorld(Creature* creature) override;

  bool CanCreatureGossipHello(Player* player, Creature* creature) override;

  bool CanCreatureGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override;
};

// Sets up the SoloQ NPC's shared creature template at startup.
class SoloqNpcSetup : public WorldScript
{
public:
  SoloqNpcSetup() : WorldScript("arenacraft::soloq::SoloqNpcSetup", {WORLDHOOK_ON_STARTUP}) {}

  void OnStartup() override;
};

// Drives SoloqService::tick from the world update loop, keeps the real 5v5
// battleground queue in sync, and drains any matches.
class SoloqDriver : public WorldScript
{
public:
  SoloqDriver() : WorldScript("arenacraft::soloq::SoloqDriver", {WORLDHOOK_ON_UPDATE}) {}

  void OnUpdate(uint32 diff) override;
};

// Stops the core from ever matching our 5v5 rated queue entries. Soloq does its
// own matchmaking, so the arena battleground must never be created by the core.
class SoloqBattlegroundScript : public AllBattlegroundScript
{
public:
  SoloqBattlegroundScript() : AllBattlegroundScript("arenacraft::soloq::SoloqBattlegroundScript") {}

  bool OnQueueUpdateValidity(BattlegroundQueue* queue, uint32 diff, BattlegroundTypeId bgTypeId,
                             BattlegroundBracketId bracket_id, uint8 arenaType, bool isRated,
                             uint32 arenaRating) override;

  void OnBattlegroundEnd(Battleground* bg, TeamId winner) override;
  void OnBattlegroundDestroy(Battleground* bg) override;
  void OnBattlegroundRemovePlayerAtLeave(Battleground* bg, Player* player) override;
};
} // namespace arenacraft::soloq
