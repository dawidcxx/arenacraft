#include "CosmeticVendor.hpp"

// Flat, code-defined "Exotic mounts" vendor stock: one row per mount item,
// served by CosmeticVendor's "Exotic mounts" option via AllExoticMounts().
// Placeholder list for now - the real popular-mount ids go here. Row order is
// the vendor order.

namespace arenacraft
{
std::vector<uint32> const& AllExoticMounts()
{
  static std::vector<uint32> const items = {
      33809, // Amani War Bear
      47180, // Argent Warhorse
      44225, // Reins of the Armored Brown Bear
      45596, // Silvermoon Hawkstrider
      45586, // Ironforge Ram
      46747, // Turbostrider
      46746, // White Skeletal Warhorse
      19872, // Swift Razzashi Raptor
      23193, // Naxxramas Deathcharger Reins
  };
  return items;
}
} // namespace arenacraft
