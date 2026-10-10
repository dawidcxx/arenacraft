#pragma once

#include "AllCreatureScript.h"
#include "Creature.h"
#include "DatabaseScript.h"
#include "GlobalScript.h"
#include "WorldScript.h"

namespace arenacraft
{
// Hijacks Doomclaw (entry 19738) into a custom elite encounter: level 80,
// elite-ranked, pinned 170k health, triple melee damage, a periodic Cloud of
// Disease with the occasional "*braap*", and a long-cast Venom Bolt nuke.
// Further buffs belong in this file.
class Doomclaw : public AllCreatureScript
{
public:
  static constexpr uint32 Entry  = 19738;
  static constexpr uint32 Level  = 80;
  static constexpr uint32 Health = 1000000;
  // Guaranteed drop, added to his loot template from code.
  static constexpr uint32 DropItem = 32859;
  static constexpr float  MeleeDamageMultiplier = 24.0f;
  static constexpr uint32 SpellCloudOfDisease   = 41193;
  static constexpr uint32 SpellVenomBolt        = 54970;
  static constexpr uint32 CloudIntervalMs       = 30 * 1000;
  // Staggered so the two casts alternate rather than landing together.
  static constexpr uint32 VenomBoltIntervalMs     = 30 * 1000;
  static constexpr uint32 VenomBoltInitialDelayMs = 15 * 1000;
  static constexpr uint32 BraapChancePercent      = 50;

  Doomclaw() : AllCreatureScript("arenacraft::Doomclaw") {}

  // Runs at the end of every SelectLevel (initial create, respawn, entry
  // change), after the core has recomputed the base health, so the pinned
  // value survives all of them.
  void Creature_SelectLevel(const CreatureTemplate* cinfo, Creature* creature) override;

  // Replaces the DB SmartAI with an AI that keeps the smart scripts and adds
  // the Cloud of Disease / Venom Bolt timers.
  [[nodiscard]] CreatureAI* GetCreatureAI(Creature* creature) const override;
};

// Promotes the shared template to elite as soon as it is loaded, before
// CheckCreatureTemplate (so the elite damage multiplier applies) and before any
// map spawns. Reapplied on every template reload.
class DoomclawSetup : public DatabaseScript
{
public:
  DoomclawSetup() : DatabaseScript("arenacraft::DoomclawSetup", {DATABASEHOOK_ON_AFTER_DATABASE_LOAD_CREATURETEMPLATES})
  {
  }

  void OnAfterDatabaseLoadCreatureTemplates(std::vector<CreatureTemplate*> creatureTemplates) override;
};

// Server-side DBC tweaks for Doomclaw's spells: a 5s Venom Bolt cast dealing a
// flat ~10k. Applied while the spell definitions are being loaded (after DBC
// corrections, before any casters exist).
class DoomclawSpellTweaks : public GlobalScript
{
public:
  DoomclawSpellTweaks() : GlobalScript("arenacraft::DoomclawSpellTweaks", {GLOBALHOOK_ON_LOAD_SPELL_CUSTOM_ATTR}) {}

  void OnLoadSpellCustomAttr(SpellInfo* spell) override;
};

// Appends the code-defined 100% drop to Doomclaw's loot template once the loot
// tables are loaded. Like the item vendors, this is code-only stock (no SQL).
class DoomclawLoot : public WorldScript
{
public:
  DoomclawLoot() : WorldScript("arenacraft::DoomclawLoot", {WORLDHOOK_ON_STARTUP}) {}

  void OnStartup() override;
};
} // namespace arenacraft
