-- Fingerprints (src/game/Arenacraft/fingerprint, ai-doc/fingerprint.md): snapshots of a player's macros, key
-- bindings and addons taken by game masters (.fingerprint touch), compared against with .fingerprint compare.
CREATE TABLE IF NOT EXISTS `fingerprint` (
  `id`         int unsigned NOT NULL AUTO_INCREMENT,
  `account_id` int unsigned NOT NULL,
  `guid`       int unsigned NOT NULL COMMENT 'characters.guid the snapshot was taken from',
  `name`       varchar(12)  NOT NULL,
  `reason`     varchar(255) NOT NULL DEFAULT '',
  `created_by` varchar(32)  NOT NULL DEFAULT '',
  `created_at` datetime     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `macros`     mediumtext   NOT NULL COMMENT 'macros-cache (account + character)',
  `bindings`   mediumtext   NOT NULL COMMENT 'bindings-cache (account + character)',
  `addons`     text         NOT NULL COMMENT 'enabled addons, one per line',
  PRIMARY KEY (`id`),
  KEY `idx_account` (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='GM fingerprints of players';

-- The addons each account's client reported at its last login, so fingerprints work for offline players too.
CREATE TABLE IF NOT EXISTS `account_addon_list` (
  `account_id` int unsigned NOT NULL,
  `addons`     text         NOT NULL COMMENT 'enabled addons, one per line',
  `updated_at` datetime     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Last reported addon list per account';
