#include "CosmeticVendor.hpp"
#include "Doomclaw.hpp"
#include "GeneralGoodsVendor.hpp"
#include "ItemVendor.hpp"
#include "soloq/SoloqNpc.hpp"
#include "transmog/Transmogrification.hpp"

void AddArenacraftScripts()
{
  new arenacraft::ItemVendor();
  new arenacraft::ItemVendorStock();
  new arenacraft::GeneralGoodsVendor();
  new arenacraft::GeneralGoodsVendorStock();
  new arenacraft::CosmeticVendor();
  new arenacraft::CosmeticVendorStock();
  new arenacraft::Doomclaw();
  new arenacraft::DoomclawSetup();
  new arenacraft::DoomclawSpellTweaks();
  new arenacraft::transmog::TransmogPlayerScript();
  new arenacraft::soloq::SoloqNpc();
  new arenacraft::soloq::SoloqNpcSetup();
  new arenacraft::soloq::SoloqDriver();
  new arenacraft::soloq::SoloqBattlegroundScript();
}
