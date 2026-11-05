-- Autocast choices for guardian pets (Feral Spirit wolves, ...): guardians are created fresh on every summon, so the
-- choice a player makes on the pet bar is kept here per character, creature entry and spell
-- (src/game/Arenacraft/PetAutocast.hpp). Real pets keep theirs with the pet as before.
CREATE TABLE IF NOT EXISTS `character_pet_autocast` (
  `guid`    int unsigned      NOT NULL COMMENT 'characters.guid of the owner',
  `entry`   mediumint unsigned NOT NULL COMMENT 'creature_template.entry of the guardian',
  `spell`   int unsigned      NOT NULL,
  `enabled` tinyint unsigned  NOT NULL,
  PRIMARY KEY (`guid`, `entry`, `spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Guardian pet autocast choices';
