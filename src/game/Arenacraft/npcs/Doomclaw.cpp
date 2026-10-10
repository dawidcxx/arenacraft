#include "Doomclaw.hpp"

#include "CreatureAI.h"
#include "DBCStores.h"
#include "LootMgr.h"
#include "ObjectMgr.h"
#include "Random.h"
#include "SharedDefines.h"
#include "SmartAI.h"
#include "SpellInfo.h"

namespace arenacraft
{
namespace
{
// Keeps the DB SmartAI behaviour (Claw Swipe, Slime Spray, ...) and layers the
// custom encounter casts on top.
class DoomclawAI : public SmartAI
{
public:
  explicit DoomclawAI(Creature* creature) : SmartAI(creature) {}

  void JustRespawned() override
  {
    SmartAI::JustRespawned();
    _inCombat = false;
  }

  void UpdateAI(uint32 diff) override
  {
    SmartAI::UpdateAI(diff);

    // Only act while fighting; arm the timers on the first combat tick so the
    // casts are staggered from the start of each engagement.
    if (!me->IsInCombat())
    {
      _inCombat = false;
      return;
    }

    if (!_inCombat)
    {
      _inCombat       = true;
      _cloudTimer     = Doomclaw::CloudIntervalMs;
      _venomBoltTimer = Doomclaw::VenomBoltInitialDelayMs;
    }

    TickCloudOfDisease(diff);
    TickVenomBolt(diff);
  }

private:
  void TickCloudOfDisease(uint32 diff)
  {
    if (_cloudTimer > diff)
    {
      _cloudTimer -= diff;
      return;
    }

    _cloudTimer = Doomclaw::CloudIntervalMs;
    me->CastSpell(me, Doomclaw::SpellCloudOfDisease, false);

    if (urand(1, 100) <= Doomclaw::BraapChancePercent)
      me->Say("*braap*", LANG_UNIVERSAL);
  }

  void TickVenomBolt(uint32 diff)
  {
    if (_venomBoltTimer > diff)
    {
      _venomBoltTimer -= diff;
      return;
    }

    _venomBoltTimer = Doomclaw::VenomBoltIntervalMs;

    if (Unit* victim = me->GetVictim())
      me->CastSpell(victim, Doomclaw::SpellVenomBolt, false);
  }

  bool   _inCombat       = false;
  uint32 _cloudTimer     = Doomclaw::CloudIntervalMs;
  uint32 _venomBoltTimer = Doomclaw::VenomBoltInitialDelayMs;
};
} // namespace

CreatureAI* Doomclaw::GetCreatureAI(Creature* creature) const
{
  if (creature->GetEntry() != Entry)
    return nullptr;

  return new DoomclawAI(creature);
}

void Doomclaw::Creature_SelectLevel(const CreatureTemplate* /*cinfo*/, Creature* creature)
{
  if (creature->GetEntry() != Entry)
    return;

  creature->SetCreateHealth(Health);
  creature->SetMaxHealth(Health);
  creature->SetHealth(Health);
  // Keep the base modifier in sync so aura-driven recomputes (UpdateMaxHealth)
  // do not fall back to the core-calculated health.
  creature->SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, float(Health));
}

void DoomclawSetup::OnAfterDatabaseLoadCreatureTemplates(std::vector<CreatureTemplate*> creatureTemplates)
{
  if (Doomclaw::Entry >= creatureTemplates.size())
    return;

  CreatureTemplate* proto = creatureTemplates[Doomclaw::Entry];
  if (!proto)
    return;

  proto->rank     = CREATURE_ELITE_ELITE;
  proto->minlevel = uint8(Doomclaw::Level);
  proto->maxlevel = uint8(Doomclaw::Level);

  // Runs before CheckCreatureTemplate(), which then multiplies in the (1.0x)
  // elite damage rate, so this is the final multiplier.
  proto->DamageModifier *= Doomclaw::MeleeDamageMultiplier;
}

void DoomclawSpellTweaks::OnLoadSpellCustomAttr(SpellInfo* spell)
{
  if (spell->Id != Doomclaw::SpellVenomBolt)
    return;

  // SpellCastTimes.dbc 6 == 5000 ms, default Venom Bolt cast is 1.5s.
  spell->CastTimeEntry = sSpellCastTimesStore.LookupEntry(6);

  // DieSides 1 makes CalcValue read BasePoints + 1, so this lands a flat ~10k.
  spell->Effects[EFFECT_0].DieSides   = 1;
  spell->Effects[EFFECT_0].BasePoints = 10000;
}

void DoomclawLoot::OnStartup()
{
  CreatureTemplate const* proto = sObjectMgr->GetCreatureTemplate(Doomclaw::Entry);
  if (!proto || !proto->lootid)
    return;

  LootTemplate* loot = LootTemplates_Creature.GetLootForConditionFill(proto->lootid);
  if (!loot)
    return;

  // groupid 0 / reference 0 => ordinary guaranteed entry, 100% chance.
  // lootmode must be LOOT_MODE_DEFAULT (DB-loaded rows are forced to it too),
  // otherwise LootTemplate::Process skips the entry as a mode mismatch.
  loot->AddEntry(new LootStoreItem(Doomclaw::DropItem, 0, 100.0f, false, LOOT_MODE_DEFAULT, 0, 1, 1));
}
} // namespace arenacraft
