#include "Composition.hpp"

#include "Battleground.h"
#include "Chat.h"
#include "Player.h"
#include "Roles.hpp"
#include "SoloqService.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace arenacraft::soloq
{
namespace
{
std::string_view roleName(Role role)
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
  return "Unknown";
}

std::string formatPlayer(QueuedPlayer const& player)
{
  std::optional<Role> const role = roleFor(player.classId, player.specIndex);

  std::string line(specName(player.classId, player.specIndex));
  line += ' ';
  line += classAbbreviation(player.classId);
  line += " (";
  line += role ? roleName(*role) : std::string_view("Unknown");
  line += ')';
  return line;
}
} // namespace

std::string_view classAbbreviation(Classes classId)
{
  switch (classId)
  {
  case CLASS_WARRIOR:
    return "War";
  case CLASS_PALADIN:
    return "Pala";
  case CLASS_HUNTER:
    return "Hunt";
  case CLASS_ROGUE:
    return "Rogue";
  case CLASS_PRIEST:
    return "Priest";
  case CLASS_DEATH_KNIGHT:
    return "DK";
  case CLASS_SHAMAN:
    return "Sham";
  case CLASS_MAGE:
    return "Mage";
  case CLASS_WARLOCK:
    return "Lock";
  case CLASS_DRUID:
    return "Druid";
  default:
    return "Unknown";
  }
}

std::string_view specName(Classes classId, uint8_t specIndex)
{
  switch (classId)
  {
  case CLASS_WARRIOR:
    switch (specIndex)
    {
    case 0:
      return "Arms";
    case 1:
      return "Fury";
    case 2:
      return "Prot";
    }
    break;
  case CLASS_PALADIN:
    switch (specIndex)
    {
    case 0:
      return "Holy";
    case 1:
      return "Prot";
    case 2:
      return "Ret";
    }
    break;
  case CLASS_HUNTER:
    switch (specIndex)
    {
    case 0:
      return "BM";
    case 1:
      return "MM";
    case 2:
      return "Surv";
    }
    break;
  case CLASS_ROGUE:
    switch (specIndex)
    {
    case 0:
      return "Assa";
    case 1:
      return "Combat";
    case 2:
      return "Sub";
    }
    break;
  case CLASS_PRIEST:
    switch (specIndex)
    {
    case 0:
      return "Disc";
    case 1:
      return "Holy";
    case 2:
      return "Shadow";
    }
    break;
  case CLASS_DEATH_KNIGHT:
    switch (specIndex)
    {
    case 0:
      return "Blood";
    case 1:
      return "Frost";
    case 2:
      return "Unholy";
    }
    break;
  case CLASS_SHAMAN:
    switch (specIndex)
    {
    case 0:
      return "Ele";
    case 1:
      return "Enh";
    case 2:
      return "Resto";
    }
    break;
  case CLASS_MAGE:
    switch (specIndex)
    {
    case 0:
      return "Arcane";
    case 1:
      return "Fire";
    case 2:
      return "Frost";
    }
    break;
  case CLASS_WARLOCK:
    switch (specIndex)
    {
    case 0:
      return "Affli";
    case 1:
      return "Demo";
    case 2:
      return "Destro";
    }
    break;
  case CLASS_DRUID:
    switch (specIndex)
    {
    case 0:
      return "Balance";
    case 1:
      return "Feral";
    case 2:
      return "Resto";
    }
    break;
  default:
    break;
  }
  return "Unknown";
}

std::string formatTeamComposition(Team const& team)
{
  std::array<QueuedPlayer const*, 3> const players = {&team.healer, &team.melee, &team.caster};

  std::string line;
  for (std::size_t i = 0; i < players.size(); ++i)
  {
    if (i)
      line += ", ";
    line += formatPlayer(*players[i]);
  }
  return line;
}

void announceEnemyComposition(Battleground* bg)
{
  if (!bg || !bg->IsSoloq())
    return;

  std::optional<SoloqService::PendingArena> const pending = SoloqService::instance().pendingArena(bg->GetInstanceID());
  if (!pending)
    return;

  std::string const allianceNotice = "Facing Enemy Team: " + formatTeamComposition(pending->match.b);
  std::string const hordeNotice    = "Facing Enemy Team: " + formatTeamComposition(pending->match.a);

  for (auto const& entry : bg->GetPlayers())
  {
    Player* player = entry.second;
    if (!player || !player->GetSession())
      continue;

    // Team A is invited to TEAM_ALLIANCE and team B to TEAM_HORDE (see
    // CreateArenaForMatch), so each side gets the other side's composition.
    if (player->GetBgTeamId() == TEAM_ALLIANCE)
      ChatHandler(player->GetSession()).SendSysMessage(allianceNotice);
    else if (player->GetBgTeamId() == TEAM_HORDE)
      ChatHandler(player->GetSession()).SendSysMessage(hordeNotice);
  }
}
} // namespace arenacraft::soloq
