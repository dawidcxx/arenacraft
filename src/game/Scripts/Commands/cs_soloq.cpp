/*
 * Player and admin command for the solo-queue subsystem.
 *
 *   .soloq join     join the queue from anywhere (players; creates a team if needed)
 *   .soloq status   role breakdown of the shared queue and whether a match can form
 *   .soloq list     every queued player with role/class/spec/rating/MMR/wait
 *   .soloq pending  arenas waiting for a result
 *   .soloq clear    dequeue everyone (clears badges too)
 *   .soloq debug    toggle bypass of the character readiness checks
 */

#include "Chat.h"
#include "CommandScript.h"
#include "DBCStores.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "SoloqArenaQueue.hpp"
#include "SoloqQueue.hpp"
#include "SoloqService.hpp"
#include "SoloqTeam.hpp"
#include "World.h"

#include <array>
#include <string>

using namespace Acore::ChatCommands;
using namespace arenacraft::soloq;

namespace
{
char const* RoleName(Role role)
{
  switch (role)
  {
  case Role::Melee:
    return "Melee";
  case Role::Caster:
    return "Caster";
  case Role::Healer:
    return "Healer";
  }
  return "?";
}

std::string ClassName(Classes classId)
{
  if (ChrClassesEntry const* entry = sChrClassesStore.LookupEntry(classId))
    return entry->name[sWorld->GetDefaultDbcLocale()];
  return "Unknown";
}
} // namespace

class soloq_commandscript : public CommandScript
{
public:
  soloq_commandscript() : CommandScript("soloq_commandscript") {}

  ChatCommandTable GetCommands() const override
  {
    static ChatCommandTable soloqTable = {
        {"join", HandleJoinCommand, SEC_PLAYER, Console::No},
        {"status", HandleStatusCommand, SEC_ADMINISTRATOR, Console::Yes},
        {"list", HandleListCommand, SEC_ADMINISTRATOR, Console::Yes},
        {"pending", HandlePendingCommand, SEC_ADMINISTRATOR, Console::Yes},
        {"clear", HandleClearCommand, SEC_ADMINISTRATOR, Console::Yes},
        {"debug", HandleDebugCommand, SEC_ADMINISTRATOR, Console::Yes},
    };
    static ChatCommandTable commandTable = {{"soloq", soloqTable}};
    return commandTable;
  }

  static bool HandleJoinCommand(ChatHandler* handler)
  {
    Player* player = handler->GetPlayer();
    if (!player)
      return false;

    if (!GetSoloqTeamInfo(player))
      CreateSoloqTeam(player);

    JoinSoloqQueue(player);
    return true;
  }

  static bool HandleStatusCommand(ChatHandler* handler)
  {
    std::vector<SoloqQueue::QueueSnapshot> const queue = SoloqService::instance().snapshot();

    std::array<uint32, 3> roles{};
    for (SoloqQueue::QueueSnapshot const& entry : queue)
      ++roles[static_cast<std::size_t>(entry.role)];

    handler->PSendSysMessage("SoloQ: {} players queued, {} pending arena(s).", queue.size(),
                             SoloqService::instance().pendingArenas().size());
    handler->PSendSysMessage("Roles: melee {} | caster {} | healer {}", roles[0], roles[1], roles[2]);

    bool const rolesReady = roles[0] >= 2 && roles[1] >= 2 && roles[2] >= 2;
    if (rolesReady)
      handler->SendSysMessage("SoloQ: requirements met, a match will form on the next world tick.");
    else
      handler->SendSysMessage("SoloQ: need at least 2 melee, 2 caster and 2 healer to form a match.");
    return true;
  }

  static bool HandleListCommand(ChatHandler* handler)
  {
    std::vector<SoloqQueue::QueueSnapshot> const queue = SoloqService::instance().snapshot();
    if (queue.empty())
    {
      handler->SendSysMessage("SoloQ queue is empty.");
      return true;
    }

    handler->PSendSysMessage("SoloQ queue ({}):", queue.size());
    for (SoloqQueue::QueueSnapshot const& entry : queue)
      handler->PSendSysMessage(" - {} {} spec{} {} rating {} mmr {} waited {}s", entry.id,
                               ClassName(entry.classId).c_str(), entry.specIndex, RoleName(entry.role), entry.rating,
                               entry.mmr, entry.waited.count() / 1000);
    return true;
  }

  static bool HandlePendingCommand(ChatHandler* handler)
  {
    std::vector<SoloqService::PendingArena> const pending = SoloqService::instance().pendingArenas();
    if (pending.empty())
    {
      handler->SendSysMessage("SoloQ: no pending arenas.");
      return true;
    }

    for (SoloqService::PendingArena const& arena : pending)
    {
      handler->PSendSysMessage("SoloQ arena instance {}:", arena.bgInstanceId);
      handler->PSendSysMessage("  A: {} {} {}", arena.match.a.melee.id, arena.match.a.caster.id,
                               arena.match.a.healer.id);
      handler->PSendSysMessage("  B: {} {} {}", arena.match.b.melee.id, arena.match.b.caster.id,
                               arena.match.b.healer.id);
    }
    return true;
  }

  static bool HandleClearCommand(ChatHandler* handler)
  {
    std::vector<PlayerId> const waiting = SoloqService::instance().waitingPlayers();
    for (PlayerId const id : waiting)
    {
      if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid{id}))
        LeaveArenaQueue(player);
      SoloqService::instance().leave(id);
    }

    handler->PSendSysMessage("SoloQ: cleared {} queued players.", waiting.size());
    return true;
  }

  static bool HandleDebugCommand(ChatHandler* handler, bool enable)
  {
    SoloqService::instance().setSkipCharacterChecks(enable);
    if (enable)
      handler->SendSysMessage(
          "SoloQ: debug enabled - readiness checks (gear/enchants/gems/talents/glyphs) are skipped when queueing.");
    else
      handler->SendSysMessage("SoloQ: debug disabled - readiness checks are enforced again.");
    return true;
  }
};

void AddSC_soloq_commandscript() { new soloq_commandscript(); }
