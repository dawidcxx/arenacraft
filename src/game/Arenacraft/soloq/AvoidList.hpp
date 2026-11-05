#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// SoloQ avoid list (#77): a player names up to two characters the matchmaker must never put on their team. Pure
// logic - characters are low guids, time is unix seconds passed in - so it is unit-tested without a world.
//
// Each of the two slots has a 6 hour cooldown that starts when the slot is filled. Removing a player empties the slot
// but keeps its cooldown, so the list can't be churned to dodge players one after another (and to farm rating).
// Only one side needs to avoid the other.
namespace arenacraft::soloq
{
struct AvoidSlot
{
  uint32_t avoided = 0; // low guid of the avoided character, 0 = empty
  int64_t  usedAt  = 0; // unix seconds the slot was last filled, 0 = never
};

class AvoidList
{
public:
  static constexpr std::size_t Slots               = 2;
  static constexpr int64_t     SlotCooldownSeconds = 6 * 60 * 60;

  using OwnerSlots = std::array<AvoidSlot, Slots>;

  enum class AddResult : uint8_t
  {
    Added,
    Self,           // can't avoid yourself
    AlreadyAvoided, // already on the list
    NoFreeSlot,     // both slots taken or still cooling down
  };

  // Fills the first free slot (empty and off cooldown). `slot` receives the slot used.
  AddResult add(uint32_t owner, uint32_t avoided, int64_t now, std::size_t* slot = nullptr);
  // Empties the slot holding `avoided`; its cooldown keeps running. `slot` receives the slot emptied.
  bool remove(uint32_t owner, uint32_t avoided, std::size_t* slot = nullptr);

  // True if either character avoids the other.
  [[nodiscard]] bool avoids(uint32_t a, uint32_t b) const;
  [[nodiscard]] bool listed(uint32_t owner, uint32_t avoided) const;

  [[nodiscard]] OwnerSlots slots(uint32_t owner) const;
  // Seconds until slot `index` of `owner` can be filled again (0 = now); the slot may still hold a player.
  [[nodiscard]] int64_t cooldownLeft(uint32_t owner, std::size_t index, int64_t now) const;

  // Restores a slot as stored (database load).
  void load(uint32_t owner, std::size_t index, AvoidSlot slot);

private:
  [[nodiscard]] static bool free(AvoidSlot const& slot, int64_t now);

  std::unordered_map<uint32_t, OwnerSlots> _slots;
};

// The last teammates of each character (newest first), so the avoid menu can offer them without typing a name. Kept in
// memory only: a restart forgets them, which is fine for a convenience list.
class RecentTeammates
{
public:
  static constexpr std::size_t Kept = 10;

  struct Mate
  {
    uint32_t    guid;
    std::string name;
  };

  // `team` = the three players of one side of a match.
  void                            record(std::array<Mate, 3> const& team);
  [[nodiscard]] std::vector<Mate> of(uint32_t player) const;

private:
  std::unordered_map<uint32_t, std::deque<Mate>> _mates;
};

// "3h 12m" / "45m" / "now" for the cooldown shown in the menu.
std::string formatCooldown(int64_t seconds);
} // namespace arenacraft::soloq
