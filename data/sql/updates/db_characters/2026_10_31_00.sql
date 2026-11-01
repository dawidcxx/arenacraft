-- Arena replays (src/game/Arenacraft/replay). One row per recorded match; the
-- packet stream is a zlib-compressed blob (ReplayFormat.cpp). Participants are
-- kept apart for `.replay list <name>` and the per-player retention limit.
CREATE TABLE IF NOT EXISTS `arena_replay` (
  `id`          int unsigned     NOT NULL,
  `bracket`     tinyint unsigned NOT NULL COMMENT '1=2v2 2=3v3 3=5v5 4=soloq 5=skirmish',
  `map_id`      int unsigned     NOT NULL,
  `bg_type_id`  int unsigned     NOT NULL,
  `winner`      tinyint unsigned NOT NULL COMMENT 'TeamId: 0 first team, 1 second team, 2 none',
  `duration_ms` int unsigned     NOT NULL,
  `created_at`  datetime         NOT NULL,
  `data`        longblob         NOT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_created_at` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Arena match recordings';

CREATE TABLE IF NOT EXISTS `arena_replay_player` (
  `replay_id` int unsigned     NOT NULL,
  `guid`      int unsigned     NOT NULL,
  `name`      varchar(12)      NOT NULL,
  `team`      tinyint unsigned NOT NULL,
  `class`     tinyint unsigned NOT NULL,
  PRIMARY KEY (`replay_id`, `guid`),
  KEY `idx_guid` (`guid`),
  KEY `idx_name` (`name`),
  CONSTRAINT `fk_arena_replay_player_replay` FOREIGN KEY (`replay_id`) REFERENCES `arena_replay` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Arena replay participants';
