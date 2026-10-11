#include "Doomclaw.hpp"

#include "Cell.h"
#include "CellImpl.h"
#include "CreatureAI.h"
#include "DBCStores.h"
#include "GridNotifiers.h"
#include "LootMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Random.h"
#include "SharedDefines.h"
#include "SmartAI.h"
#include "Spell.h"
#include "SpellInfo.h"

namespace arenacraft
{
namespace
{
// Mirrors Spell::EffectInterruptCast: true if the unit is casting or channeling
// a spell that the stomp's interrupt effect would actually stop.
bool IsInterruptibleCast(Unit* unit)
{
  for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_AUTOREPEAT_SPELL; ++i)
  {
    Spell* spell = unit->GetCurrentSpell(CurrentSpellTypes(i));
    if (!spell)
      continue;

    SpellInfo const* info       = spell->GetSpellInfo();
    bool             inProgress = spell->getState() == SPELL_STATE_CASTING ||
                      (spell->getState() == SPELL_STATE_PREPARING && spell->GetCastTime() > 0.0f);
    if (inProgress && info->PreventionType == SPELL_PREVENTION_TYPE_SILENCE &&
        ((i == CURRENT_GENERIC_SPELL && (info->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT)) ||
         (i == CURRENT_CHANNELED_SPELL && (info->ChannelInterruptFlags & CHANNEL_INTERRUPT_FLAG_INTERRUPT))))
      return true;
  }
  return false;
}

// Keeps the DB SmartAI behaviour (Claw Swipe, Slime Spray, ...) and layers the
// custom encounter casts on top.
class DoomclawAI : public SmartAI
{
public:
  explicit DoomclawAI(Creature* creature) : SmartAI(creature) {}

  void JustRespawned() override
  {
    SmartAI::JustRespawned();
    _inCombat        = false;
    _stompBonusDelay = 0;
    _stompBonusTargets.clear();
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
      _stompTimer     = Doomclaw::StompInitialDelayMs;
    }

    TickCloudOfDisease(diff);
    TickVenomBolt(diff);
    TickStaggeringStomp(diff);
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

  void TickStaggeringStomp(uint32 diff)
  {
    TickStompBonus(diff);

    if (_stompTimer > diff)
    {
      _stompTimer -= diff;
      return;
    }

    _stompTimer = Doomclaw::StompIntervalMs;

    // Snapshot who is mid-cast in the stomp's damage radius; the spell will
    // interrupt them a moment later.
    _stompBonusTargets.clear();
    std::list<Unit*>                                                    enemies;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck                          check(me, me, Doomclaw::StompRadiusYards);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(me, enemies, check);
    Cell::VisitAllObjects(me, searcher, Doomclaw::StompRadiusYards);
    for (Unit* enemy : enemies)
      if (IsInterruptibleCast(enemy))
        _stompBonusTargets.push_back(enemy->GetGUID());

    me->CastSpell(me, Doomclaw::SpellStaggeringStomp, false);

    // The spell is a 0.5s cast; award the bonus just after it lands.
    if (!_stompBonusTargets.empty())
      _stompBonusDelay = 600;
  }

  void TickStompBonus(uint32 diff)
  {
    if (!_stompBonusDelay)
      return;

    if (_stompBonusDelay > diff)
    {
      _stompBonusDelay -= diff;
      return;
    }

    _stompBonusDelay = 0;
    for (ObjectGuid guid : _stompBonusTargets)
    {
      Unit* target = ObjectAccessor::GetUnit(*me, guid);
      if (!target || !target->IsAlive() || !me->IsWithinDistInMap(target, Doomclaw::StompRadiusYards))
        continue;

      // The spell already landed one hit; add two more so the total is tripled.
      uint32 bonus = uint32(urand(uint32(Doomclaw::StompDamageMin), uint32(Doomclaw::StompDamageMax))) * 2;
      Unit::DealDamage(me, target, bonus, nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
    }
    _stompBonusTargets.clear();
  }

  bool                    _inCombat        = false;
  uint32                  _cloudTimer      = Doomclaw::CloudIntervalMs;
  uint32                  _venomBoltTimer  = Doomclaw::VenomBoltInitialDelayMs;
  uint32                  _stompTimer      = Doomclaw::StompInitialDelayMs;
  uint32                  _stompBonusDelay = 0;
  std::vector<ObjectGuid> _stompBonusTargets;
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
  if (spell->Id == Doomclaw::SpellVenomBolt)
  {
    // SpellCastTimes.dbc 6 == 5000 ms, default Venom Bolt cast is 1.5s.
    spell->CastTimeEntry = sSpellCastTimesStore.LookupEntry(6);

    // DieSides 1 makes CalcValue read BasePoints + 1, so this lands a flat ~10k.
    spell->Effects[EFFECT_0].DieSides   = 1;
    spell->Effects[EFFECT_0].BasePoints = 10000;
  }
  else if (spell->Id == Doomclaw::SpellStaggeringStomp)
  {
    // CalcValue rolls BasePoints + irand(1, DieSides).
    spell->Effects[EFFECT_0].BasePoints = Doomclaw::StompDamageMin - 1;
    spell->Effects[EFFECT_0].DieSides   = Doomclaw::StompDamageMax - Doomclaw::StompDamageMin + 1;
  }
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
