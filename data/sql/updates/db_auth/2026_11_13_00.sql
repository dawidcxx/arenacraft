-- DB update 2026_11_10_00 -> 2026_11_13_00
--
-- Personal OBS overlay links, managed by the Discord bot (`/overlay`). One link per
-- account: the token is the secret part of `<site>/overlay/<token>`, `guid` is the
-- SoloQ character the overlay shows. The website reads this table to resolve a token.

CREATE TABLE IF NOT EXISTS `account_overlay` (
  `account_id` int unsigned NOT NULL,
  `token` char(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `guid` int unsigned NOT NULL COMMENT 'acore_characters.characters.guid shown on the overlay',
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`account_id`),
  UNIQUE KEY `uk_account_overlay_token` (`token`),
  CONSTRAINT `fk_account_overlay_account` FOREIGN KEY (`account_id`) REFERENCES `account` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Personal OBS overlay links';
