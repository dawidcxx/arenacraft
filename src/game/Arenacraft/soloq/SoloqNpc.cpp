#include "SoloqNpc.hpp"

#include "Arena.h"
#include "ArenaTeam.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "CharacterCache.h"
#include "CharacterCheck.hpp"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
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
//
// A player who was invited but never accepted the arena still has a real
// m_QueuedPlayers entry at this point (its IsInvitedToBGInstanceGUID is this
// arena). Clearing only their client queue id would strand that entry (and its
// scheduled removal event keys off the queue id, so it would never clean it up).
// The next queue attempt then hits the AddGroup duplicate-player assertion.
// Remove the real entry too.
//
// A player who forfeited mid-match may have already requeued (a fresh 5v5
// entry, not invited to this arena), so only touch entries that belong to
// `bgInstanceId`; otherwise we would yank them out of their new queue when the
// old match finally ends.
void ClearSoloqQueueId(Player* player, uint32 bgInstanceId)
{
  if (!player)
    return;

  BattlegroundQueue& queue = sBattlegroundMgr->GetBattlegroundQueue(BATTLEGROUND_QUEUE_5v5);

  GroupQueueInfo ginfo;
  if (queue.GetPlayerGroupInfoData(player->GetGUID(), &ginfo))
  {
    // Queued for a different arena (requeued after forfeiting): leave it alone.
    if (ginfo.IsInvitedToBGInstanceGUID != bgInstanceId)
      return;

    // Invited to (but perhaps never entered) this arena: drop the entry.
    queue.RemovePlayer(player->GetGUID(), true);
  }

  // No entry left for this arena: just clear the participant's stale badge.
  player->RemoveBattlegroundQueueId(BATTLEGROUND_QUEUE_5v5);
}
} // namespace

namespace
{
// --- Avoid list (#77) ---------------------------------------------------------------------------------------------

int64_t Now() { return GameTime::GetGameTime().count(); }

std::string NameOf(uint32 guid)
{
  std::string name;
  if (!sCharacterCache->GetCharacterNameByGuid(ObjectGuid::Create<HighGuid::Player>(guid), name))
    name = "#" + std::to_string(guid);
  return name;
}

void SaveAvoidSlot(uint32 owner, std::size_t index, AvoidSlot const& slot)
{
  CharacterDatabase.Execute("REPLACE INTO soloq_avoid (guid, slot, avoided_guid, used_at) VALUES ({}, {}, {}, {})",
                            owner, index, slot.avoided, slot.usedAt);
}

std::string AvoidPageText(uint32 owner)
{
  AvoidList const& list = SoloqService::instance().avoidList();
  int64_t const    now  = Now();
  std::string      text = "SoloQ Avoid List\n-----------------------\n\nYou are never put on a team with these "
                          "players.\n\n";
  AvoidList::OwnerSlots const slots = list.slots(owner);
  for (std::size_t i = 0; i < AvoidList::Slots; ++i)
  {
    int64_t const cooldown = list.cooldownLeft(owner, i, now);
    text += "Slot " + std::to_string(i + 1) + ": " + (slots[i].avoided ? NameOf(slots[i].avoided) : "empty") +
            (cooldown ? " (free to change in " + formatCooldown(cooldown) + ")" : "") + "\n";
  }
  text += "\nAdding a player starts a 6 hour cooldown on the slot. Removing a player does not end it - choose "
          "carefully.";
  return text;
}

void ShowAvoidMenu(Player* player, Creature* creature)
{
  uint32 const owner = player->GetGUID().GetCounter();
  AvoidList&   list  = SoloqService::instance().avoidList();

  player->PlayerTalkClass->ClearMenus();
  GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
  menu.SetMenuId(SoloqNpc::GossipMenuId);
  uint32 menuIndex = 0;
  auto   addOption = [&](uint8 icon, std::string const& text, uint32 action, std::string const& box, bool coded)
  {
    menu.AddMenuItem(int32(menuIndex), icon, text, 0, action, box, 0, coded);
    menu.AddGossipMenuItemData(menuIndex, 0, 0);
    ++menuIndex;
  };

  addOption(GOSSIP_ICON_CHAT, "Avoid a player by name...", SoloqNpc::ActionAvoidByName,
            "Type the character name. The slot then cools down for 6 hours.", true);

  std::vector<RecentTeammates::Mate> const recent = SoloqService::instance().recentTeammates().of(owner);
  for (std::size_t i = 0; i < recent.size(); ++i)
    if (!list.listed(owner, recent[i].guid))
      addOption(GOSSIP_ICON_CHAT, "Avoid " + recent[i].name + " (recent teammate)",
                SoloqNpc::ActionAvoidRecentBase + uint32(i),
                "Avoid " + recent[i].name + "? The slot then cools down for 6 hours.", false);

  AvoidList::OwnerSlots const slots = list.slots(owner);
  for (std::size_t i = 0; i < AvoidList::Slots; ++i)
    if (slots[i].avoided)
    {
      std::string const name = NameOf(slots[i].avoided);
      addOption(GOSSIP_ICON_INTERACT_1, "Stop avoiding " + name, SoloqNpc::ActionAvoidRemoveBase + uint32(i),
                "Stop avoiding " + name + "? The slot's cooldown keeps running.", false);
    }

  addOption(GOSSIP_ICON_CHAT, "Back", SoloqNpc::ActionAvoidBack, "", false);

  sObjectMgr->AddOrUpdateGossipText(SoloqNpc::AvoidTextId, AvoidPageText(owner));
  player->GetSession()->SendNpcTextUpdate(SoloqNpc::AvoidTextId);
  SendGossipMenuFor(player, SoloqNpc::AvoidTextId, creature);
}

void AddToAvoidList(Player* player, uint32 avoided, std::string const& name)
{
  uint32 const  owner = player->GetGUID().GetCounter();
  AvoidList&    list  = SoloqService::instance().avoidList();
  int64_t const now   = Now();
  std::size_t   slot  = 0;
  switch (list.add(owner, avoided, now, &slot))
  {
  case AvoidList::AddResult::Added:
    SaveAvoidSlot(owner, slot, list.slots(owner)[slot]);
    SendMessage(player, "SoloQ: you will not be put on a team with " + name + ".");
    break;
  case AvoidList::AddResult::Self:
    SendMessage(player, "SoloQ: you can't avoid yourself.");
    break;
  case AvoidList::AddResult::AlreadyAvoided:
    SendMessage(player, "SoloQ: " + name + " is already on your avoid list.");
    break;
  case AvoidList::AddResult::NoFreeSlot:
  {
    AvoidList::OwnerSlots const slots = list.slots(owner);
    int64_t                     wait  = -1;
    for (std::size_t i = 0; i < AvoidList::Slots; ++i)
      if (!slots[i].avoided)
      {
        int64_t const left = list.cooldownLeft(owner, i, now);
        wait               = wait < 0 ? left : std::min(wait, left);
      }
    SendMessage(player, wait < 0 ? "SoloQ: both avoid slots are taken - remove a player first."
                                 : "SoloQ: no free avoid slot, the next one frees in " + formatCooldown(wait) + ".");
    break;
  }
  }
}

void RemoveFromAvoidList(Player* player, std::size_t index)
{
  uint32 const                owner = player->GetGUID().GetCounter();
  AvoidList&                  list  = SoloqService::instance().avoidList();
  AvoidList::OwnerSlots const slots = list.slots(owner);
  if (index >= AvoidList::Slots || !slots[index].avoided)
    return;
  std::string const name = NameOf(slots[index].avoided);
  if (list.remove(owner, slots[index].avoided))
  {
    SaveAvoidSlot(owner, index, list.slots(owner)[index]);
    SendMessage(player, "SoloQ: " + name + " removed from your avoid list; the slot's cooldown keeps running.");
  }
}

void LoadAvoidLists()
{
  AvoidList& list   = SoloqService::instance().avoidList();
  uint32     loaded = 0;
  if (QueryResult result = CharacterDatabase.Query("SELECT guid, slot, avoided_guid, used_at FROM soloq_avoid"))
    do
    {
      Field* fields = result->Fetch();
      list.load(fields[0].Get<uint32>(), fields[1].Get<uint8>(),
                AvoidSlot{fields[2].Get<uint32>(), static_cast<int64_t>(fields[3].Get<uint32>())});
      ++loaded;
    } while (result->NextRow());
  LOG_INFO("server.loading", "soloq: loaded {} avoid list slots", loaded);
}

void RecordTeammates(Match const& match)
{
  auto mate = [](QueuedPlayer const& queued)
  {
    ObjectGuid const guid = ObjectGuid{queued.id};
    std::string      name;
    if (Player* player = ObjectAccessor::FindPlayer(guid))
      name = player->GetName();
    else
      sCharacterCache->GetCharacterNameByGuid(guid, name);
    return RecentTeammates::Mate{guid.GetCounter(), name};
  };
  RecentTeammates& recent = SoloqService::instance().recentTeammates();
  for (Team const* team : {&match.a, &match.b})
    recent.record({mate(team->melee), mate(team->caster), mate(team->healer)});
}
} // namespace

void SoloqNpc::OnCreatureAddWorld(Creature* creature)
{
  if (creature->GetEntry() == Entry)
    creature->ReplaceAllNpcFlags(UNIT_NPC_FLAG_GOSSIP);
}

void SoloqNpcSetup::OnStartup()
{
  LoadAvoidLists();

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
  addOption(GOSSIP_ICON_INTERACT_1, "Reset SoloQ Team", ActionReset);
  addOption(GOSSIP_ICON_TALK, "Avoid list", ActionAvoidMenu);

  // The gossip page text is built at runtime from the shared queue. It is
  // cached/registered in ObjectMgr under a custom id and pushed to the client
  // explicitly because the client caches npc text by id and would otherwise show
  // stale counts.
  RoleCounts const& counts = SoloqService::instance().roleCounts();
  sObjectMgr->AddOrUpdateGossipText(StatsTextId, formatQueueStats(counts));
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

  // avoid list pages stay open
  if (action == ActionAvoidMenu)
  {
    ShowAvoidMenu(player, creature);
    return true;
  }
  if (action == ActionAvoidBack)
  {
    player->PlayerTalkClass->SendCloseGossip();
    return CanCreatureGossipHello(player, creature);
  }
  if (action >= ActionAvoidRemoveBase && action < ActionAvoidRemoveBase + AvoidList::Slots)
  {
    RemoveFromAvoidList(player, action - ActionAvoidRemoveBase);
    ShowAvoidMenu(player, creature);
    return true;
  }
  if (action >= ActionAvoidRecentBase && action < ActionAvoidRecentBase + RecentTeammates::Kept)
  {
    std::vector<RecentTeammates::Mate> const recent = service.recentTeammates().of(player->GetGUID().GetCounter());
    std::size_t const                        index  = action - ActionAvoidRecentBase;
    if (index < recent.size())
      AddToAvoidList(player, recent[index].guid, recent[index].name);
    ShowAvoidMenu(player, creature);
    return true;
  }

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
  case ActionReset:
    LeaveArenaQueue(player);
    service.leave(id);
    if (ResetSoloqTeam(player))
    {
      SendMessage(player, "SoloQ: rating reset to " + std::to_string(tuning::InitialRating) + "; MMR kept.");
    }
    else
      SendMessage(player, "SoloQ: you do not have a team.");
    break;
  case ActionJoin:
    JoinSoloqQueue(player);
    break;
  case ActionLeave:
    LeaveArenaQueue(player);
    SendMessage(player, service.leave(id) ? "SoloQ: left the queue." : "SoloQ: you were not in the queue.");
    break;
  default:
    return false;
  }

  return true;
}

bool SoloqNpc::CanCreatureGossipSelectCode(Player* player, Creature* creature, uint32 /*sender*/, uint32 action,
                                           char const* code)
{
  if (creature->GetEntry() != Entry)
    return false;
  if (!player || action != ActionAvoidByName)
    return true;

  std::string name = code ? code : "";
  if (!normalizePlayerName(name))
  {
    SendMessage(player, "SoloQ: type a character name.");
    ShowAvoidMenu(player, creature);
    return true;
  }
  ObjectGuid const guid = sCharacterCache->GetCharacterGuidByName(name);
  if (!guid)
    SendMessage(player, "SoloQ: there is no character named " + name + ".");
  else
    AddToAvoidList(player, guid.GetCounter(), name);
  ShowAvoidMenu(player, creature);
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

    RecordTeammates(match);
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
  if (!bg)
    return;

  SoloqService& service = SoloqService::instance();

  if (winner == TEAM_NEUTRAL)
  {
    // A short-handed start is neutral-ended on purpose by
    // Arena::CancelIfIncompleteAtStart. Consume the pending match here so the
    // bot frees the room immediately with the right reason; any other neutral
    // end still falls through to OnBattlegroundDestroy as a declined match.
    Arena* arena = bg->isArena() ? static_cast<Arena*>(bg) : nullptr;
    if (!arena || !arena->IsCancelledAtStart())
      return;

    std::optional<SoloqService::PendingArena> const pending = service.takeMatch(bg->GetInstanceID());
    if (!pending)
      return;

    // Clear the badge/queue entry for everyone, including players who never
    // entered the arena (they only get removed by the invite timeout otherwise).
    for (QueuedPlayer const* queued : playersOf(pending->match))
      if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid{queued->id}))
        ClearSoloqQueueId(player, bg->GetInstanceID());

    publishMatchupEnded(
        makeMatchupEndedEvent(pending->match, bg, false, MatchupEndReason::Cancelled, pending->startedAtMs));
    return;
  }

  std::optional<SoloqService::PendingArena> const pending = service.takeMatch(bg->GetInstanceID());
  if (!pending)
    return;

  Match const&                      match   = pending->match;
  MatchResult const                 result  = winner == TEAM_ALLIANCE ? MatchResult::TeamAWin : MatchResult::TeamBWin;
  std::array<RatingUpdate, 6> const updates = resolveMatch(match, result);

  publishMatchupEnded(
      makeMatchupEndedEvent(match, bg, true, MatchupEndReason::Finished, pending->startedAtMs, winner, &updates));

  for (std::size_t i = 0; i < updates.size(); ++i)
  {
    RatingUpdate const& update = updates[i];
    Player*             player = ObjectAccessor::FindPlayer(ObjectGuid{update.id});
    if (!player)
      continue;

    ClearSoloqQueueId(player, bg->GetInstanceID());

    bool const won     = (result == MatchResult::TeamAWin) == (i < 3);
    bool const applied = ApplySoloqResult(player, update.rating, update.mmr, won);
    if (!applied)
      continue;

    SendMessage(player, "SoloQ: rating " + std::to_string(update.rating) + " (" + (update.ratingDelta >= 0 ? "+" : "") +
                            std::to_string(update.ratingDelta) + "), MMR " + std::to_string(update.mmr) + " (" +
                            (update.mmrDelta >= 0 ? "+" : "") + std::to_string(update.mmrDelta) + ").");
  }
}

void SoloqBattlegroundScript::OnBattlegroundDestroy(Battleground* bg)
{
  if (!bg)
    return;

  // If the arena was destroyed without ever finishing (invites declined), the
  // match is still pending here - notify consumers so they can free the room.
  std::optional<SoloqService::PendingArena> const pending = SoloqService::instance().takeMatch(bg->GetInstanceID());
  if (pending)
    publishMatchupEnded(
        makeMatchupEndedEvent(pending->match, bg, false, MatchupEndReason::Declined, pending->startedAtMs));
}

void SoloqBattlegroundScript::OnBattlegroundRemovePlayerAtLeave(Battleground* bg, Player* player)
{
  if (!bg || !bg->isArena())
    return;

  ClearSoloqQueueId(player, bg->GetInstanceID());
}
} // namespace arenacraft::soloq
