#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <utility>

// Shared proc rolls. Inside one proc event - a spell landing on one target (Spell::DoAllEffectOnTarget) or one
// melee/ranged swing (Unit::ProcDamageAndSpell) - every proc of the same unit with the same chance uses one roll, so
// they happen together or not at all: Frostbite and Fingers of Frost (both 15% at 3/3 and 2/2) proc together, two
// trinkets with the same chance co-occur. Procs with different chances, of different units or of different events
// stay independent. Outside an event a roll is a plain independent roll.
namespace arenacraft
{
// The rolls of one event: (owner, chance in 1/100 %) -> outcome. Pure, unit-tested with a fake random source.
class SharedProcRolls
{
public:
  // `random` returns a value in [0, 100).
  explicit SharedProcRolls(std::function<float()> random) : _random(std::move(random)) {}

  bool roll(void const* owner, float chance);

private:
  std::function<float()>                          _random;
  std::map<std::pair<void const*, int32_t>, bool> _outcomes;
};

// Opens a proc event for the current thread. `Join` reuses the event already open (the procs of a spell hit are
// handled inside the hit), `New` always starts a fresh one (a spell triggered from a proc is its own event).
class ProcRollScope
{
public:
  enum Mode
  {
    New,
    Join,
  };

  explicit ProcRollScope(Mode mode = New);
  ~ProcRollScope();
  ProcRollScope(ProcRollScope const&)            = delete;
  ProcRollScope& operator=(ProcRollScope const&) = delete;

private:
  std::optional<SharedProcRolls> _rolls; // empty when joining an open event
  SharedProcRolls*               _previous = nullptr;
};

// Rolls `chance` (percent) for a proc of `owner`, shared within the current event.
bool RollProcChance(void const* owner, float chance);
} // namespace arenacraft
