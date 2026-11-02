-- VIP characters (src/game/Arenacraft/vip). One row per character flagged VIP by
-- an administrator through `.vip add`; loaded into memory once at startup.
CREATE TABLE IF NOT EXISTS `character_vip` (
  `guid`       int unsigned NOT NULL COMMENT 'characters.guid',
  `created_at` datetime     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='VIP characters';
