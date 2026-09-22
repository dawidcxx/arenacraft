#include "SoloqNpc.hpp"

#include "ArenaTeam.h"
#include "Battleground.h"
#include "CharacterCheck.hpp"
#include "Chat.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Outcome.hpp"
#include "SharedDefines.h"
#include "SoloqArenaQueue.hpp"
#include "SoloqEvents.hpp"
#include "SoloqService.hpp"
#include "SoloqTeam.hpp"
#include "WorldSession.h"

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace arenacraft::soloq
{
namespace
{
void SendMessage(Player* player, std::string const& message)
{
  if (player && player->GetSession())
    ChatHandler(player->GetSession()).SendSysMessage(message);
}

// The soloq badge/queue track is 5v5, but the match instance is created as 3v3.
// The core's leave cleanup therefore drops the (unused) 3v3 queue id and leaves
// the 5v5 one set, keeping the player "in queue" and blocking requeue. Clear it
// explicitly when the match ends and when a player leaves the arena.
void ClearSoloqQueueId(Player* player)
{
  if (player && player->InBattlegroundQueueForBattlegroundQueueType(BATTLEGROUND_QUEUE_5v5))
    player->RemoveBattlegroundQueueId(BATTLEGROUND_QUEUE_5v5);
}
} // namespace

void SoloqNpc::OnCreatureAddWorld(Creature* creature)
{
  if (creature->GetEntry() == Entry)
    creature->ReplaceAllNpcFlags(UNIT_NPC_FLAG_GOSSIP);
}

void SoloqNpcSetup::OnStartup()
{
  if (CreatureTemplate* proto = const_cast<CreatureTemplate*>(sObjectMgr->GetCreatureTemplate(SoloqNpc::Entry)))
    proto->SubName = "SoloQ Master";
}

bool SoloqNpc::CanCreatureGossipHello(Player* player, Creature* creature)
{
  if (creature->GetEntry() != Entry)
    return false;

  player->PlayerTalkClass->ClearMenus();
  GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
  menu.SetMenuId(GossipMenuId);

  uint32 menuIndex = 0;
  auto   addOption = [&](uint8 icon, char const* text, uint32 action)
  {
    menu.AddMenuItem(int32(menuIndex), icon, text, 0, action, "", 0, false);
    menu.AddGossipMenuItemData(menuIndex, 0, 0);
    ++menuIndex;
  };

  addOption(GOSSIP_ICON_BATTLE, "Join SoloQ", ActionJoin);
  addOption(GOSSIP_ICON_CHAT, "Leave SoloQ", ActionLeave);
  addOption(GOSSIP_ICON_INTERACT_1, "Create SoloQ Team", ActionCreate);
  addOption(GOSSIP_ICON_INTERACT_1, "Delete SoloQ Team", ActionDelete);

  // The gossip page text is built at runtime and shows both faction queues, so
  // players learn that each faction has its own queue. It is cached/registered
  // in ObjectMgr under a custom id and pushed to the client explicitly because
  // the client caches npc text by id and would otherwise show stale counts.
  std::array<RoleCounts, 2> const& counts = SoloqService::instance().factionRoleCounts();
  sObjectMgr->AddOrUpdateGossipText(StatsTextId, formatQueueStats(counts[TEAM_HORDE], counts[TEAM_ALLIANCE]));
  player->GetSession()->SendNpcTextUpdate(StatsTextId);

  SendGossipMenuFor(player, StatsTextId, creature);
  return true;
}

bool SoloqNpc::CanCreatureGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
{
  if (creature->GetEntry() != Entry)
    return false;

  if (!player)
    return true;

  PlayerId const id      = player->GetGUID().GetRawValue();
  SoloqService&  service = SoloqService::instance();

  player->PlayerTalkClass->SendCloseGossip();

  switch (action)
  {
  case ActionCreate:
    if (player->GetArenaTeamId(ARENA_SLOT_5v5) != 0)
      SendMessage(player, "SoloQ: you already have a 5v5 team.");
    else if (CreateSoloqTeam(player))
      SendMessage(player, "SoloQ: team created (rating " + std::to_string(tuning::InitialRating) + ", MMR " +
                              std::to_string(tuning::InitialMmr) + "). It now shows in your PvP pane.");
    else
      SendMessage(player, "SoloQ: could not create a team.");
    break;
  case ActionDelete:
    LeaveArenaQueue(player);
    service.leave(id);
    SendMessage(player, DeleteSoloqTeam(player) ? "SoloQ: team deleted; rating and MMR reset."
                                                : "SoloQ: you do not have a team.");
    break;
  case ActionJoin:
  {
    std::optional<SoloqTeamInfo> const team = GetSoloqTeamInfo(player);
    if (!team)
      SendMessage(player, "SoloQ: create a team first.");
    else if (service.inQueue(id))
      SendMessage(player, "SoloQ: you are already in the queue.");
    else if (player->InBattlegroundQueue())
      SendMessage(player, "SoloQ: leave your current battleground or arena queue first.");
    else if (std::optional<CharacterProblem> const problem = service.characterProblem(player))
      SendMessage(player, std::string("SoloQ: ") + describeCharacterProblem(*problem));
    else if (!service.join(id, static_cast<Classes>(player->getClass()), player->GetMostPointsTalentTree(),
                           player->GetTeamId(), team->rating, team->mmr))
      SendMessage(player, "SoloQ: could not join the queue.");
    else if (!EnterArenaQueue(player))
    {
      service.leave(id);
      SendMessage(player, "SoloQ: could not join the queue right now.");
    }
    else
      SendMessage(player, "SoloQ: joined the queue (" + std::to_string(service.queueSize()) + " waiting).");
    break;
  }
  case ActionLeave:
    LeaveArenaQueue(player);
    SendMessage(player, service.leave(id) ? "SoloQ: left the queue." : "SoloQ: you were not in the queue.");
    break;
  default:
    return false;
  }

  return true;
}

void SoloqDriver::OnUpdate(uint32 diff)
{
  SoloqService& service = SoloqService::instance();

  // The client can leave through the battleground queue UI and players can log
  // out; drop anyone no longer registered in the 5v5 queue so we never match a
  // stale entry.
  for (PlayerId const id : service.waitingPlayers())
  {
    Player* player = ObjectAccessor::FindPlayer(ObjectGuid{id});
    if (!player || !InArenaQueue(player))
      service.leave(id);
  }

  std::vector<Match> const matches = service.tick(std::chrono::milliseconds(diff));

  for (Match const& match : matches)
  {
    LOG_INFO("server", "soloq: match formed: [{} {} {}] vs [{} {} {}]", match.a.melee.id, match.a.caster.id,
             match.a.healer.id, match.b.melee.id, match.b.caster.id, match.b.healer.id);

    if (!CreateArenaForMatch(match))
    {
      LOG_ERROR("server", "soloq: could not create an arena for a formed match; dropping the players from the queue.");
      for (QueuedPlayer const* queued : playersOf(match))
        if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid{queued->id}))
          LeaveArenaQueue(player);
      continue;
    }

    for (QueuedPlayer const* queued : playersOf(match))
      if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid{queued->id}))
        SendMessage(player, "SoloQ: match found - accept the arena invite to enter.");
  }
}

bool SoloqBattlegroundScript::OnQueueUpdateValidity(BattlegroundQueue* /*queue*/, uint32 /*diff*/,
                                                    BattlegroundTypeId /*bgTypeId*/,
                                                    BattlegroundBracketId /*bracket_id*/, uint8 arenaType, bool isRated,
                                                    uint32 /*arenaRating*/)
{
  return !(isRated && arenaType == ARENA_TYPE_5v5);
}

void SoloqBattlegroundScript::OnBattlegroundEnd(Battleground* bg, TeamId winner)
{
  if (!bg || winner == TEAM_NEUTRAL)
    return;

  SoloqService&              service = SoloqService::instance();
  std::optional<Match> const match   = service.takeMatch(bg->GetInstanceID());
  if (!match)
    return;

  publishMatchupEnded(makeMatchupEndedEvent(*match, bg, true));

  MatchResult const                 result  = winner == TEAM_ALLIANCE ? MatchResult::TeamAWin : MatchResult::TeamBWin;
  std::array<RatingUpdate, 6> const updates = resolveMatch(*match, result);

  for (std::size_t i = 0; i < updates.size(); ++i)
  {
    RatingUpdate const& update = updates[i];
    Player*             player = ObjectAccessor::FindPlayer(ObjectGuid{update.id});
    if (!player)
      continue;

    ClearSoloqQueueId(player);

    bool const won = (result == MatchResult::TeamAWin) == (i < 3);
    ApplySoloqResult(player, update.rating, update.mmr, won);

    SendMessage(player, "SoloQ: rating " + std::to_string(update.rating) + " (" + (update.delta >= 0 ? "+" : "") +
                            std::to_string(update.delta) + "), MMR " + std::to_string(update.mmr) + ".");
  }
}

void SoloqBattlegroundScript::OnBattlegroundDestroy(Battleground* bg)
{
  if (!bg)
    return;

  // If the arena was destroyed without ever finishing (invites declined), the
  // match is still pending here - notify consumers so they can free the room.
  std::optional<Match> const match = SoloqService::instance().takeMatch(bg->GetInstanceID());
  if (match)
    publishMatchupEnded(makeMatchupEndedEvent(*match, bg, false));
}

void SoloqBattlegroundScript::OnBattlegroundRemovePlayerAtLeave(Battleground* bg, Player* player)
{
  if (!bg || !bg->isArena())
    return;

  ClearSoloqQueueId(player);
}
} // namespace arenacraft::soloq
