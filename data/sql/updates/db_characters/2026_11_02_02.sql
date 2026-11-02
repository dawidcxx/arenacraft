-- Patrol (src/game/Arenacraft/patrol, ai-doc/patrol.md): player reports of arena replays, reviewed anonymously
-- by other players. One case per (replay, suspect); it closes after AC_PATROL_VERDICTS verdicts.
CREATE TABLE IF NOT EXISTS `patrol_case` (
  `id`           int unsigned     NOT NULL AUTO_INCREMENT,
  `replay_id`    int unsigned     NOT NULL COMMENT 'arena_replay.id',
  `suspect_guid` int unsigned     NOT NULL COMMENT 'characters.guid',
  `suspect_name` varchar(12)      NOT NULL,
  `suspect_rating` int unsigned   NOT NULL DEFAULT 0 COMMENT 'solo queue rating when reported (AC_PATROL_RATING_WINDOW)',
  `categories`   tinyint unsigned NOT NULL DEFAULT 0 COMMENT 'reported categories, bit per PatrolCategory',
  `status`       tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0 open, 1 banned, 2 cleared, 3 guilty but not punished',
  `guilty`       tinyint unsigned NOT NULL DEFAULT 0 COMMENT 'categories that reached the ban threshold',
  `created_at`   datetime         NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `closed_at`    datetime         NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uniq_replay_suspect` (`replay_id`, `suspect_guid`),
  KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Patrol cases';

CREATE TABLE IF NOT EXISTS `patrol_report` (
  `case_id`          int unsigned     NOT NULL,
  `reporter_account` int unsigned     NOT NULL,
  `reporter_guid`    int unsigned     NOT NULL,
  `category`         tinyint unsigned NOT NULL,
  `created_at`       datetime         NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`case_id`, `reporter_account`),
  KEY `idx_reporter_time` (`reporter_account`, `created_at`),
  CONSTRAINT `fk_patrol_report_case` FOREIGN KEY (`case_id`) REFERENCES `patrol_case` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Patrol reports';

CREATE TABLE IF NOT EXISTS `patrol_verdict` (
  `case_id`    int unsigned     NOT NULL,
  `account_id` int unsigned     NOT NULL,
  `guid`       int unsigned     NOT NULL,
  `skipped`    tinyint unsigned NOT NULL DEFAULT 0 COMMENT '1 = passed on the case, does not count as a verdict',
  `yes_mask`   tinyint unsigned NOT NULL DEFAULT 0 COMMENT 'categories answered yes, bit per PatrolCategory',
  `created_at` datetime         NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`case_id`, `account_id`),
  CONSTRAINT `fk_patrol_verdict_case` FOREIGN KEY (`case_id`) REFERENCES `patrol_case` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Patrol verdicts';
