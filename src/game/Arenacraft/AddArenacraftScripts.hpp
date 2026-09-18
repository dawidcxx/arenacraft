#include "soloq/soloq_scripts.hpp"
#include "vendor_scripts.hpp"

void AddArenacraftScripts()
{
  new arenacraft::soloq::SoloQueueScript();
  new arenacraft::soloq::NpcSoloq();
  new arenacraft::vendors::ShaulyPoreVendor();
}