#include "CosmeticVendor.hpp"
#include "GeneralGoodsVendor.hpp"
#include "ItemVendor.hpp"
#include "soloq/SoloqNpc.hpp"

void AddArenacraftScripts()
{
  new arenacraft::ItemVendor();
  new arenacraft::ItemVendorStock();
  new arenacraft::GeneralGoodsVendor();
  new arenacraft::GeneralGoodsVendorStock();
  new arenacraft::CosmeticVendor();
  new arenacraft::CosmeticVendorStock();
  new arenacraft::soloq::SoloqNpc();
  new arenacraft::soloq::SoloqNpcSetup();
  new arenacraft::soloq::SoloqDriver();
  new arenacraft::soloq::SoloqBattlegroundScript();
}
