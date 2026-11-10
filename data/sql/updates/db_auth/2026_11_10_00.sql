-- DB update 2026_11_03_00 -> 2026_11_10_00
--
-- Tournament seating tiers, managed by the Discord bot. A missing row means the
-- default `None` seat; only the elevated `Gold` and `Diamond` seats are stored.
-- reset_soloq reads this table (auth DB) and starts the linked account's SoloQ
-- characters at the matching MMR for the new season.

CREATE TABLE IF NOT EXISTS `account_seat` (
  `account_id` int unsigned NOT NULL,
  `tier` enum('Gold','Diamond') NOT NULL,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`account_id`),
  KEY `idx_account_seat_tier_updated` (`tier`, `updated_at`),
  CONSTRAINT `fk_account_seat_account` FOREIGN KEY (`account_id`) REFERENCES `account` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Tournament seating tiers for SoloQ season resets';
