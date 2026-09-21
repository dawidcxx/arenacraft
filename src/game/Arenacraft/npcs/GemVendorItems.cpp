#include "ItemVendor.hpp"

// Flat, code-defined gem vendor stock. The single "Gems" gossip option serves
// the ordinary WotLK epic socketables (AllGems): the cut gems of the six
// WotLK epic families, grouped by socket colour (red, blue, yellow, purple,
// orange, green) and kept in that block order. The separate "Meta Gems" option
// serves AllMetaGems: every WotLK-tier (level 80) meta gem.
//
// Source: the WotLK gem listing on wotlk.evowow.com (?items=3 for the coloured
// gems, ?items=3.6 for the metas). Only quality-epic (q4) WotLK gems are kept:
// the rare WotLK gems, every BC-era gem and the other pre-WotLK gems are
// skipped. Also skipped are the special non-ordinary WotLK gems - the
// jewelcrafting-only unique Dragon's Eye cuts, Stormjewel and Kharmaa's Grace -
// so the list stays the standard cut gems. The prismatic Nightmare Tear is
// included in the meta list (it matches any socket).
// Every id was checked on evowow and is available to players.
//
// Vendor order follows row order, so moving a row up moves it up in the vendor.

namespace arenacraft
{
std::vector<uint32> const& AllGems()
{
  static std::vector<uint32> const gems = {
      // == Red (Cardinal Ruby) ==
      40111, // Bold Cardinal Ruby
      40112, // Delicate Cardinal Ruby
      40113, // Runed Cardinal Ruby
      40114, // Bright Cardinal Ruby
      40115, // Subtle Cardinal Ruby
      40116, // Flashing Cardinal Ruby
      40117, // Fractured Cardinal Ruby
      40118, // Precise Cardinal Ruby
      // == Blue (Majestic Zircon) ==
      40119, // Solid Majestic Zircon
      40120, // Sparkling Majestic Zircon
      40121, // Lustrous Majestic Zircon
      40122, // Stormy Majestic Zircon
      // == Yellow (King's Amber) ==
      40123, // Brilliant King's Amber
      40124, // Smooth King's Amber
      40125, // Rigid King's Amber
      40126, // Thick King's Amber
      40127, // Mystic King's Amber
      40128, // Quick King's Amber
      // == Purple (Dreadstone) ==
      40129, // Sovereign Dreadstone
      40130, // Shifting Dreadstone
      40131, // Tenuous Dreadstone
      40132, // Glowing Dreadstone
      40133, // Purified Dreadstone
      40134, // Royal Dreadstone
      40135, // Mysterious Dreadstone
      40136, // Balanced Dreadstone
      40137, // Infused Dreadstone
      40138, // Regal Dreadstone
      40139, // Defender's Dreadstone
      40140, // Puissant Dreadstone
      40141, // Guardian's Dreadstone
      // == Orange (Ametrine) ==
      40142, // Inscribed Ametrine
      40143, // Etched Ametrine
      40144, // Champion's Ametrine
      40145, // Resplendent Ametrine
      40146, // Fierce Ametrine
      40147, // Deadly Ametrine
      40148, // Glinting Ametrine
      40149, // Lucent Ametrine
      40150, // Deft Ametrine
      40151, // Luminous Ametrine
      40152, // Potent Ametrine
      40153, // Veiled Ametrine
      40154, // Durable Ametrine
      40155, // Reckless Ametrine
      40156, // Wicked Ametrine
      40157, // Pristine Ametrine
      40158, // Empowered Ametrine
      40159, // Stark Ametrine
      40160, // Stalwart Ametrine
      40161, // Glimmering Ametrine
      40162, // Accurate Ametrine
      40163, // Resolute Ametrine
      // == Green (Eye of Zul) ==
      40164, // Timeless Eye of Zul
      40165, // Jagged Eye of Zul
      40166, // Vivid Eye of Zul
      40167, // Enduring Eye of Zul
      40168, // Steady Eye of Zul
      40169, // Forceful Eye of Zul
      40170, // Seer's Eye of Zul
      40171, // Misty Eye of Zul
      40172, // Shining Eye of Zul
      40173, // Turbid Eye of Zul
      40174, // Intricate Eye of Zul
      40175, // Dazzling Eye of Zul
      40176, // Sundered Eye of Zul
      40177, // Lambent Eye of Zul
      40178, // Opaque Eye of Zul
      40179, // Energized Eye of Zul
      40180, // Radiant Eye of Zul
      40181, // Tense Eye of Zul
      40182, // Shattered Eye of Zul
  };
  return gems;
}

std::vector<uint32> const& AllMetaGems()
{
  static std::vector<uint32> const metas = {
      41266, // Skyflare Diamond
      41285, // Chaotic Skyflare Diamond
      41307, // Destructive Skyflare Diamond
      41333, // Ember Skyflare Diamond
      41334, // Earthsiege Diamond
      41335, // Enigmatic Skyflare Diamond
      41339, // Swift Skyflare Diamond
      41375, // Tireless Skyflare Diamond
      41376, // Revitalizing Skyflare Diamond
      41377, // Effulgent Skyflare Diamond
      41378, // Forlorn Skyflare Diamond
      41379, // Impassive Skyflare Diamond
      41380, // Austere Earthsiege Diamond
      41381, // Persistent Earthsiege Diamond
      41382, // Trenchant Earthsiege Diamond
      41385, // Invigorating Earthsiege Diamond
      41389, // Beaming Earthsiege Diamond
      41395, // Bracing Earthsiege Diamond
      41396, // Eternal Earthsiege Diamond
      41397, // Powerful Earthsiege Diamond
      41398, // Relentless Earthsiege Diamond
      41400, // Thundering Skyflare Diamond
      41401, // Insightful Earthsiege Diamond
      44076, // Swift Starflare Diamond
      44078, // Tireless Starflare Diamond
      44081, // Enigmatic Starflare Diamond
      44082, // Impassive Starflare Diamond
      44084, // Forlorn Starflare Diamond
      44087, // Persistent Earthshatter Diamond
      44088, // Powerful Earthshatter Diamond
      44089, // Trenchant Earthshatter Diamond
      49110, // Nightmare Tear (prismatic, matches any socket)
  };
  return metas;
}
} // namespace arenacraft
