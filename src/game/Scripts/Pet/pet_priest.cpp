/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "npc_pet_pri_".
 */

#include "CharmInfo.h"
#include "CreatureScript.h"
#include "PetAI.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "TotemAI.h"

enum PriestSpells
{
  SPELL_PRIEST_GLYPH_OF_SHADOWFIEND      = 58228,
  SPELL_PRIEST_GLYPH_OF_SHADOWFIEND_MANA = 58227,
  SPELL_PRIEST_SHADOWFIEND_DODGE         = 8273,
  SPELL_PRIEST_LIGHTWELL_CHARGES         = 59907
};

struct npc_pet_pri_lightwell : public TotemAI
{
  npc_pet_pri_lightwell(Creature* creature) : TotemAI(creature) {}

  void InitializeAI() override
  {
    if (TempSummon* tempSummon = me->ToTempSummon())
    {
      if (Unit* owner = tempSummon->GetSummonerUnit())
      {
        uint32 hp = uint32(owner->GetMaxHealth() * 0.3f);
        me->SetMaxHealth(hp);
        me->SetHealth(hp);
        me->SetLevel(owner->GetLevel());
      }
    }

    me->CastSpell(me, SPELL_PRIEST_LIGHTWELL_CHARGES, false); // Spell for Lightwell Charges
    TotemAI::InitializeAI();
  }
};

struct npc_pet_pri_shadowfiend : public PetAI
{
  npc_pet_pri_shadowfiend(Creature* creature) : PetAI(creature) {}

  void Reset() override
  {
    PetAI::Reset();
    if (!me->HasAura(SPELL_PRIEST_SHADOWFIEND_DODGE))
      me->AddAura(SPELL_PRIEST_SHADOWFIEND_DODGE, me);

    _engagedOnSpawn = false;
  }

  void UpdateAI(uint32 diff) override
  {
    PetAI::UpdateAI(diff);

    // The guardian summon sequence finishes by issuing a follow (Spell::SummonGuardian), which
    // overrides any chase started during spawn - a player-controlled guardian's Attack never flags
    // it "in combat", so the summon always falls into that follow branch. Re-issue the owner's
    // target once after that, the equivalent of a /petattack, so the fiend commits to the fight
    // instead of walking straight back to the summoner. Shadowcrawl (already on autocast) then
    // warps it onto the target.
    if (_engagedOnSpawn || me->GetVictim() || me->HasReactState(REACT_PASSIVE))
      return;

    Player* owner  = me->GetOwner() ? me->GetOwner()->ToPlayer() : nullptr;
    Unit*   target = owner ? owner->GetSelectedUnit() : nullptr;

    if (!owner || !target || !owner->IsValidAttackTarget(target) || !me->GetCharmInfo())
      return;

    _engagedOnSpawn = true;

    me->ClearUnitState(UNIT_STATE_FOLLOW);
    me->AttackStop();

    me->GetCharmInfo()->SetIsCommandAttack(true);
    me->GetCharmInfo()->SetIsAtStay(false);
    me->GetCharmInfo()->SetIsFollowing(false);
    me->GetCharmInfo()->SetIsCommandFollow(false);
    me->GetCharmInfo()->SetIsReturning(false);

    AttackStart(target);
  }

  void JustDied(Unit* /*killer*/) override
  {
    if (me->IsSummon())
      if (Unit* owner = me->ToTempSummon()->GetSummonerUnit())
        if (owner->HasAura(SPELL_PRIEST_GLYPH_OF_SHADOWFIEND))
          owner->CastSpell(owner, SPELL_PRIEST_GLYPH_OF_SHADOWFIEND_MANA, true);
  }

private:
  bool _engagedOnSpawn = false;
};

void AddSC_priest_pet_scripts()
{
  RegisterCreatureAI(npc_pet_pri_lightwell);
  RegisterCreatureAI(npc_pet_pri_shadowfiend);
}
