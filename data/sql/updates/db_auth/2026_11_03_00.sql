-- DB update 2026_11_01_00 -> 2026_11_03_00
--
-- Links a WoW account to the immutable Discord user id that owns it. The bot
-- used to key accounts off the mutable Discord username, so a renamed/reclaimed
-- handle could reset someone else's password. Apply with scripts/db_sync.

CREATE TABLE IF NOT EXISTS `account_discord` (
  `account_id` int unsigned NOT NULL,
  `discord_id` varchar(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `linked_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`account_id`),
  UNIQUE KEY `idx_discord_id` (`discord_id`),
  CONSTRAINT `fk_account_discord_account` FOREIGN KEY (`account_id`) REFERENCES `account` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Immutable Discord identity linked to a WoW account';
