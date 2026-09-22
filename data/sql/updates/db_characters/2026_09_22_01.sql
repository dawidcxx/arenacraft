-- Per-item transmogrification appearances. `FakeEntry` is the item id whose
-- look is shown in place of the real equipped item; the item itself is never
-- modified, so stats and set bonuses are untouched. Keyed by the item's global
-- GUID (item_instance.guid is a global counter, never reused).
CREATE TABLE IF NOT EXISTS `character_transmogrification` (
  `ItemGuid`  int unsigned NOT NULL,
  `OwnerGuid` int unsigned NOT NULL,
  `FakeEntry` int unsigned NOT NULL,
  PRIMARY KEY (`ItemGuid`),
  KEY `idx_owner_guid` (`OwnerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Transmogrification appearances';
