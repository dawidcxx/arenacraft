-- DB update 2026_11_04_00 -> 2026_11_05_00
--
-- SoloQ avoid list (#77, src/game/Arenacraft/soloq/AvoidList.hpp, ai-doc/soloq.md): two slots per character. A slot
-- keeps its cooldown start (used_at) after the player in it is removed, so the row stays with avoided_guid = 0.

CREATE TABLE IF NOT EXISTS `soloq_avoid` (
  `guid`         int unsigned     NOT NULL COMMENT 'characters.guid of the list owner',
  `slot`         tinyint unsigned NOT NULL,
  `avoided_guid` int unsigned     NOT NULL DEFAULT 0 COMMENT 'characters.guid avoided, 0 = slot empty',
  `used_at`      int unsigned     NOT NULL DEFAULT 0 COMMENT 'unix time the slot was last filled (cooldown start)',
  PRIMARY KEY (`guid`, `slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='SoloQ avoid list slots';
