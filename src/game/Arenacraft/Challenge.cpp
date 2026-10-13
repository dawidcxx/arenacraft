#include "Challenge.hpp"

#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "Chat.h"
#include "CommandScript.h"
#include "DBCStores.h"
#include "Group.h"
#include "Map.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SocialMgr.h"

#include <cstddef>
#include <string>
#include <vector>

using namespace Acore::ChatCommands;

namespace arenacraft
{
namespace
{
// A side is the challenger/target alone, or their normal party. Party sizes
// above this (or non-normal groups) are rejected. Side size 1 plays a 1v1, size
// 2 a 2v2.
constexpr std::size_t MaxSideSize = 2;

// Fills `side` with the player and their normal party members. Returns false if
// the group is not a plain party we can use for a challenge.
bool CollectSide(Player* player, std::vector<Player*>& side)
{
  Group* group = player->GetGroup();
  if (!group)
  {
    side.push_back(player);
    return true;
  }

  if (group->isRaidGroup() || group->isBGGroup() || group->isBFGroup() || group->isLFGGroup() ||
      group->GetMembersCount() > MaxSideSize)
    return false;

  group->DoForAllMembers([&side](Player* member) { side.push_back(member); });
  return !side.empty();
}

bool Ready(Player* player)
{
  return player && player->IsInWorld() && player->FindMap() && player->IsAlive() && !player->IsBeingTeleported() &&
         !player->IsInFlight() && !player->InBattleground() && !player->InBattlegroundQueue() &&
         !player->isUsingLfg() && !player->GetMap()->GetEntry()->Instanceable();
}

void Notify(Player* player, std::string const& message)
{
  if (player && player->GetSession())
    ChatHandler(player->GetSession()).SendSysMessage(message);
}

class ChallengeCommand : public CommandScript
{
public:
  ChallengeCommand() : CommandScript("arenacraft::ChallengeCommand") {}

  ChatCommandTable GetCommands() const override
  {
    static ChatCommandTable table = {{"challenge", HandleChallenge, SEC_PLAYER, Console::No}};
    return table;
  }

private:
  static bool HandleChallenge(ChatHandler* handler, Optional<PlayerIdentifier> target)
  {
    Player* challenger = handler->GetPlayer();
    if (!challenger)
      return false;

    if (!target || !target->IsConnected())
    {
      handler->SendErrorMessage("Name an online player to challenge.");
      return false;
    }

    Player* enemy = target->GetConnectedPlayer();
    if (enemy == challenger)
    {
      handler->SendErrorMessage("You cannot challenge yourself.");
      return false;
    }

    // Both sides are built from the players' parties, so a party member would
    // end up queued on both teams.
    if (challenger->GetGroup() && challenger->GetGroup() == enemy->GetGroup())
    {
      handler->SendErrorMessage("You cannot challenge a member of your own party. Leave the party first.");
      return false;
    }

    // Build both sides from their normal parties and require equal sizes.
    std::vector<Player*> mySide;
    std::vector<Player*> enemySide;
    if (!CollectSide(challenger, mySide))
    {
      handler->SendErrorMessage("Only a party of one or two can challenge.");
      return false;
    }
    if (!CollectSide(enemy, enemySide))
    {
      handler->SendErrorMessage("Only a party of one or two can be challenged.");
      return false;
    }
    if (mySide.size() != enemySide.size())
    {
      handler->SendErrorMessage("Both sides must have the same number of players (1v1 or 2v2).");
      return false;
    }

    for (Player* member : mySide)
      if (!Ready(member))
      {
        handler->SendErrorMessage("Your side is not ready (battleground, queue, instance or dead).");
        return false;
      }
    for (Player* member : enemySide)
      if (!Ready(member))
      {
        handler->SendErrorMessage("The other side is not ready right now.");
        return false;
      }

    // A challenged player who ignores anyone on the challenging side cannot be
    // challenged.
    for (Player* defender : enemySide)
      for (Player* attacker : mySide)
        if (defender->GetSocial() && defender->GetSocial()->HasIgnore(attacker->GetGUID()))
        {
          handler->SendErrorMessage("You cannot challenge {} right now.", enemy->GetName());
          return false;
        }

    Battleground* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
    if (!bgTemplate)
    {
      handler->SendErrorMessage("Could not start the arena right now.");
      return false;
    }

    PvPDifficultyEntry const* bracketEntry =
        GetBattlegroundBracketByLevel(bgTemplate->GetMapId(), challenger->GetLevel());
    if (!bracketEntry)
    {
      handler->SendErrorMessage("No arena bracket for your level.");
      return false;
    }

    // 1v1 has no native bracket, so it rides the 2v2 track exactly like the GM
    // .skirmish command does; the match is still played with one player per team.
    constexpr uint8               arenaType = ARENA_TYPE_2v2;
    BattlegroundQueueTypeId const queueType = BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_AA, arenaType);
    BattlegroundQueue&            bgQueue   = sBattlegroundMgr->GetBattlegroundQueue(queueType);

    GroupQueueInfo* myInfo = bgQueue.AddGroup(challenger, challenger->GetGroup(), BATTLEGROUND_AA, bracketEntry,
                                              arenaType, false, false, 0, 0);
    for (Player* member : mySide)
      member->AddBattlegroundQueueId(queueType);

    GroupQueueInfo* enemyInfo =
        bgQueue.AddGroup(enemy, enemy->GetGroup(), BATTLEGROUND_AA, bracketEntry, arenaType, false, false, 0, 0);
    for (Player* member : enemySide)
      member->AddBattlegroundQueueId(queueType);

    Battleground* arena = sBattlegroundMgr->CreateNewBattleground(BATTLEGROUND_AA, bracketEntry, arenaType, false);
    if (!arena)
    {
      for (Player* member : mySide)
      {
        bgQueue.RemovePlayer(member->GetGUID(), false);
        member->RemoveBattlegroundQueueId(queueType);
      }
      for (Player* member : enemySide)
      {
        bgQueue.RemovePlayer(member->GetGUID(), false);
        member->RemoveBattlegroundQueueId(queueType);
      }
      handler->SendErrorMessage("Could not start the arena right now.");
      return false;
    }

    // Challenger's faction gets one team, the target the other (arenas mix
    // factions, so this only picks the sides).
    TeamId const myTeam    = challenger->GetTeamId();
    TeamId const enemyTeam = (myTeam == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE);

    bgQueue.InviteGroupToBG(myInfo, arena, myTeam);
    bgQueue.InviteGroupToBG(enemyInfo, arena, enemyTeam);
    arena->StartBattleground();

    for (Player* member : mySide)
      Notify(member, "Arena ready - accept the invite to enter.");
    for (Player* member : enemySide)
      Notify(member, challenger->GetName() + " challenged you - accept the arena invite to fight.");

    return true;
  }
};
} // namespace

void AddChallengeCommand() { new ChallengeCommand(); }
} // namespace arenacraft
