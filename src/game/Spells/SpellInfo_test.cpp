#include <doctest/doctest.h>

#include "SharedDefines.h"
#include "SpellInfo.h"

#include <memory>

namespace
{
struct TestSpellEntry
{
  SpellEntry entry{};

  TestSpellEntry()
  {
    entry.EquippedItemClass = -1;
    entry.SchoolMask        = SPELL_SCHOOL_MASK_NORMAL;
  }

  TestSpellEntry& WithAttributes(uint32 attr)
  {
    entry.Attributes = attr;
    return *this;
  }

  TestSpellEntry& WithAttributesEx(uint32 attr)
  {
    entry.AttributesEx = attr;
    return *this;
  }

  TestSpellEntry& WithEffect(uint8 effIndex, uint32 effect)
  {
    if (effIndex < MAX_SPELL_EFFECTS)
      entry.Effect[effIndex] = effect;
    return *this;
  }

  TestSpellEntry& WithEffectImplicitTargets(uint8 effIndex, uint32 targetA, uint32 targetB = 0)
  {
    if (effIndex < MAX_SPELL_EFFECTS)
    {
      entry.EffectImplicitTargetA[effIndex] = targetA;
      entry.EffectImplicitTargetB[effIndex] = targetB;
    }
    return *this;
  }

  std::unique_ptr<SpellInfo> Build() { return std::make_unique<SpellInfo>(&entry); }
};
} // namespace

TEST_CASE("SpellInfo::CanBeRedirectedBySpellMagnet allows single-target spells")
{
  auto spellInfo = TestSpellEntry()
                       .WithEffect(EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE)
                       .WithEffectImplicitTargets(EFFECT_0, TARGET_UNIT_TARGET_ENEMY)
                       .Build();

  CHECK(spellInfo->CanBeRedirectedBySpellMagnet());
}

TEST_CASE("SpellInfo::CanBeRedirectedBySpellMagnet rejects area-targeting spells")
{
  auto spellInfo = TestSpellEntry()
                       .WithEffect(EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE)
                       .WithEffectImplicitTargets(EFFECT_0, TARGET_UNIT_CASTER, TARGET_UNIT_SRC_AREA_ENEMY)
                       .Build();

  CHECK_FALSE(spellInfo->CanBeRedirectedBySpellMagnet());
}

TEST_CASE("SpellInfo::CanBeRedirectedBySpellMagnet rejects persistent area auras")
{
  auto spellInfo = TestSpellEntry().WithEffect(EFFECT_0, SPELL_EFFECT_PERSISTENT_AREA_AURA).Build();

  CHECK_FALSE(spellInfo->CanBeRedirectedBySpellMagnet());
}

TEST_CASE("SpellInfo::CanBeRedirectedBySpellMagnet rejects abilities")
{
  auto spellInfo =
      TestSpellEntry().WithAttributes(SPELL_ATTR0_IS_ABILITY).WithEffect(EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE).Build();

  CHECK_FALSE(spellInfo->CanBeRedirectedBySpellMagnet());
}

TEST_CASE("SpellInfo::CanBeRedirectedBySpellMagnet rejects no-redirection spells")
{
  auto spellInfo = TestSpellEntry()
                       .WithAttributesEx(SPELL_ATTR1_NO_REDIRECTION)
                       .WithEffect(EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE)
                       .Build();

  CHECK_FALSE(spellInfo->CanBeRedirectedBySpellMagnet());
}

TEST_CASE("SpellInfo::CanBeRedirectedBySpellMagnet rejects spells ignoring immunities")
{
  auto spellInfo = TestSpellEntry()
                       .WithAttributes(SPELL_ATTR0_NO_IMMUNITIES)
                       .WithEffect(EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE)
                       .Build();

  CHECK_FALSE(spellInfo->CanBeRedirectedBySpellMagnet());
}
