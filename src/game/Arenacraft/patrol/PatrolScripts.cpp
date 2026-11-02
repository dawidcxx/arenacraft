/*
 * Patrol commands (see Patrol.hpp):
 *
 *   .patrol accept    review a random reported match (names hidden)
 *   .patrol yes|no    answer the current question
 *   .patrol skip      pass on the current case
 *   .patrol status    what you are reviewing / whether you may patrol
 *   .patrol camera    toggle the camera following the suspect
 *   .patrol cases     (game masters) open cases with their progress
 */
#include "Chat.h"
#include "CommandScript.h"
#include "Patrol.hpp"
#include "PatrolConfig.hpp"
#include "Player.h"
#include "PlayerScript.h"
#include "WorldScript.h"

using namespace Acore::ChatCommands;

namespace arenacraft::patrol
{
namespace
{
class PatrolCommandScript : public CommandScript
{
public:
  PatrolCommandScript() : CommandScript("arenacraft::patrol::PatrolCommandScript") {}

  ChatCommandTable GetCommands() const override
  {
    static ChatCommandTable patrolTable = {
        {"accept", HandleAccept, SEC_PLAYER, Console::No},
        {"yes", HandleYes, SEC_PLAYER, Console::No},
        {"no", HandleNo, SEC_PLAYER, Console::No},
        {"skip", HandleSkip, SEC_PLAYER, Console::No},
        {"status", HandleStatus, SEC_PLAYER, Console::No},
        {"camera", HandleCamera, SEC_PLAYER, Console::No},
        {"cases", HandleCases, SEC_GAMEMASTER, Console::Yes},
    };
    static ChatCommandTable commandTable = {{"patrol", patrolTable}};
    return commandTable;
  }

  static bool HandleAccept(ChatHandler* handler)
  {
    if (Player* player = handler->GetPlayer())
      Accept(handler, player);
    return true;
  }

  static bool HandleYes(ChatHandler* handler)
  {
    if (Player* player = handler->GetPlayer())
      Answer(handler, player, true);
    return true;
  }

  static bool HandleNo(ChatHandler* handler)
  {
    if (Player* player = handler->GetPlayer())
      Answer(handler, player, false);
    return true;
  }

  static bool HandleSkip(ChatHandler* handler)
  {
    if (Player* player = handler->GetPlayer())
      Skip(handler, player);
    return true;
  }

  static bool HandleStatus(ChatHandler* handler)
  {
    if (Player* player = handler->GetPlayer())
      Status(handler, player);
    return true;
  }

  static bool HandleCamera(ChatHandler* handler)
  {
    if (Player* player = handler->GetPlayer())
      ToggleFollow(handler, player);
    return true;
  }

  static bool HandleCases(ChatHandler* handler)
  {
    ListOpenCases(handler);
    return true;
  }
};

class PatrolPlayerScript : public PlayerScript
{
public:
  PatrolPlayerScript() : PlayerScript("arenacraft::patrol::PatrolPlayerScript", {PLAYERHOOK_ON_LOGOUT}) {}

  void OnLogout(Player* player) override { patrol::OnLogout(player); }
};

// Patrol.* options are (re)read with the rest of the config, so `.reload config` applies them live.
class PatrolWorldScript : public WorldScript
{
public:
  PatrolWorldScript()
      : WorldScript("arenacraft::patrol::PatrolWorldScript", {WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP})
  {
  }

  void OnAfterConfigLoad(bool /*reload*/) override { loadConfig(); }
  void OnStartup() override { loadConfig(); }
};
} // namespace

void AddPatrolScripts()
{
  new PatrolCommandScript();
  new PatrolPlayerScript();
  new PatrolWorldScript();
}
} // namespace arenacraft::patrol
