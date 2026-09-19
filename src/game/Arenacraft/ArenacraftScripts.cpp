#include "ItemVendor.hpp"
#include "soloq/SoloqNpc.hpp"

void AddArenacraftScripts()
{
  new arenacraft::ItemVendor();
  new arenacraft::ItemVendorStock();
  new arenacraft::soloq::SoloqNpc();
  new arenacraft::soloq::SoloqDriver();
  new arenacraft::soloq::SoloqBattlegroundScript();
}
