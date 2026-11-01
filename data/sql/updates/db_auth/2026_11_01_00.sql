-- DB update 2026_09_19_01 -> 2026_11_01_00
--
-- Per-account display alias, managed by the Discord bot for accounts with the
-- VIP role. The in-game `.alias get` command resolves character -> account ->
-- alias, falling back to the account (Discord) name when no alias is set.

CREATE TABLE IF NOT EXISTS `account_alias` (
  `account_id` int unsigned NOT NULL,
  `alias` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`account_id`),
  CONSTRAINT `fk_account_alias_account` FOREIGN KEY (`account_id`) REFERENCES `account` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Discord/VIP display aliases';
