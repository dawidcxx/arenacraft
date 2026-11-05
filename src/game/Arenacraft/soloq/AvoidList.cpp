#include "AvoidList.hpp"

#include <algorithm>

namespace arenacraft::soloq
{
bool AvoidList::free(AvoidSlot const& slot, int64_t now)
{
  return slot.avoided == 0 && (slot.usedAt == 0 || now - slot.usedAt >= SlotCooldownSeconds);
}

AvoidList::AddResult AvoidList::add(uint32_t owner, uint32_t avoided, int64_t now, std::size_t* slot)
{
  if (owner == avoided)
    return AddResult::Self;
  OwnerSlots& own = _slots[owner];
  for (AvoidSlot const& entry : own)
    if (entry.avoided == avoided)
      return AddResult::AlreadyAvoided;
  for (std::size_t i = 0; i < Slots; ++i)
  {
    if (!free(own[i], now))
      continue;
    own[i] = AvoidSlot{avoided, now};
    if (slot)
      *slot = i;
    return AddResult::Added;
  }
  return AddResult::NoFreeSlot;
}

bool AvoidList::remove(uint32_t owner, uint32_t avoided, std::size_t* slot)
{
  auto it = _slots.find(owner);
  if (it == _slots.end() || !avoided)
    return false;
  for (std::size_t i = 0; i < Slots; ++i)
  {
    if (it->second[i].avoided != avoided)
      continue;
    it->second[i].avoided = 0; // usedAt stays: removing does not give the cooldown back
    if (slot)
      *slot = i;
    return true;
  }
  return false;
}

bool AvoidList::listed(uint32_t owner, uint32_t avoided) const
{
  auto it = _slots.find(owner);
  if (it == _slots.end() || !avoided)
    return false;
  return std::any_of(it->second.begin(), it->second.end(),
                     [avoided](AvoidSlot const& entry) { return entry.avoided == avoided; });
}

bool AvoidList::avoids(uint32_t a, uint32_t b) const { return listed(a, b) || listed(b, a); }

AvoidList::OwnerSlots AvoidList::slots(uint32_t owner) const
{
  auto it = _slots.find(owner);
  return it == _slots.end() ? OwnerSlots{} : it->second;
}

int64_t AvoidList::cooldownLeft(uint32_t owner, std::size_t index, int64_t now) const
{
  if (index >= Slots)
    return 0;
  AvoidSlot const slot = slots(owner)[index];
  if (slot.usedAt == 0)
    return 0;
  return std::max<int64_t>(slot.usedAt + SlotCooldownSeconds - now, 0);
}

void AvoidList::load(uint32_t owner, std::size_t index, AvoidSlot slot)
{
  if (index < Slots)
    _slots[owner][index] = slot;
}

void RecentTeammates::record(std::array<Mate, 3> const& team)
{
  for (Mate const& player : team)
  {
    std::deque<Mate>& mates = _mates[player.guid];
    for (Mate const& mate : team)
    {
      if (mate.guid == player.guid)
        continue;
      std::erase_if(mates, [&](Mate const& known) { return known.guid == mate.guid; });
      mates.push_front(mate);
    }
    while (mates.size() > Kept)
      mates.pop_back();
  }
}

std::vector<RecentTeammates::Mate> RecentTeammates::of(uint32_t player) const
{
  auto it = _mates.find(player);
  if (it == _mates.end())
    return {};
  return {it->second.begin(), it->second.end()};
}

std::string formatCooldown(int64_t seconds)
{
  if (seconds <= 0)
    return "now";
  int64_t const minutes = (seconds + 59) / 60;
  if (minutes < 60)
    return std::to_string(minutes) + "m";
  return std::to_string(minutes / 60) + "h " + std::to_string(minutes % 60) + "m";
}
} // namespace arenacraft::soloq
