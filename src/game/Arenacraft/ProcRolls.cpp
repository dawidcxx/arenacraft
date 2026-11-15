#include "ProcRolls.hpp"

#include "Random.h"

#include <cmath>

namespace arenacraft
{
bool SharedProcRolls::roll(void const* owner, float chance)
{
  if (chance <= 0.0f)
    return false;
  if (chance >= 100.0f)
    return true;

  auto const key = std::make_pair(owner, int32_t(std::lround(chance * 100.0f)));
  if (auto it = _outcomes.find(key); it != _outcomes.end())
    return it->second;
  bool const outcome = _random() < chance;
  _outcomes.emplace(key, outcome);
  return outcome;
}

namespace
{
thread_local SharedProcRolls* CurrentRolls = nullptr;
}

ProcRollScope::ProcRollScope(Mode mode)
{
  if (mode == Join && CurrentRolls)
    return;
  _previous = CurrentRolls;
  _rolls.emplace([] { return float(rand_chance()); });
  CurrentRolls = &*_rolls;
}

ProcRollScope::~ProcRollScope()
{
  if (_rolls)
    CurrentRolls = _previous;
}

bool RollProcChance(void const* owner, float chance)
{
  if (CurrentRolls)
    return CurrentRolls->roll(owner, chance);
  return roll_chance_f(chance);
}
} // namespace arenacraft
