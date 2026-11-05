-- SoloQ role of each character (src/game/Arenacraft/soloq): written when the
-- character joins the queue, from the class + main talent tree it queued with.
-- Read by apps/api to tag ladder rows as Melee / Caster / Healer.
CREATE TABLE IF NOT EXISTS `character_soloq_role` (
  `guid`       int unsigned     NOT NULL COMMENT 'characters.guid',
  `role`       tinyint unsigned NOT NULL COMMENT '0=melee 1=caster 2=healer (arenacraft::soloq::Role)',
  `updated_at` datetime         NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Last SoloQ role per character';
