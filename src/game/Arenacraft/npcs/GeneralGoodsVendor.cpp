#include "GeneralGoodsVendor.hpp"

#include "ItemVendor.hpp"
#include "ObjectMgr.h"

#include <vector>

namespace arenacraft
{
void GeneralGoodsVendor::OnCreatureAddWorld(Creature* creature)
{
  if (creature->GetEntry() != Entry)
    return;

  creature->ReplaceAllNpcFlags(UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_REPAIR);
}

void GeneralGoodsVendorStock::OnStartup()
{
  // Drop the stock the DB shipped for this entry so the list is exactly the
  // shared "General goods" one. Code-defined only (persist = false).
  if (VendorItemData const* existing = sObjectMgr->GetNpcVendorItemList(GeneralGoodsVendor::Entry))
  {
    std::vector<uint32> items;
    items.reserve(existing->m_items.size());
    for (VendorItem const* vendorItem : existing->m_items)
      items.push_back(vendorItem->item);

    for (uint32 item : items)
      sObjectMgr->RemoveVendorItem(GeneralGoodsVendor::Entry, item, false);
  }

  for (uint32 item : AllGeneralGoods())
    StockVendorItem(GeneralGoodsVendor::Entry, item);

  if (CreatureTemplate* proto =
          const_cast<CreatureTemplate*>(sObjectMgr->GetCreatureTemplate(GeneralGoodsVendor::Entry)))
    proto->SubName = "General goods";
}
} // namespace arenacraft
