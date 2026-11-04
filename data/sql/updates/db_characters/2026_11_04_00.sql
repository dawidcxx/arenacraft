-- #65: death knights were given the NPC Death Coil chain (62900-62904) on top of the player one (47541, 49892-49895),
-- so the spellbook showed the NPC ability. It is no longer granted (src/game/Arenacraft/StartingSpells.cpp); remove
-- it from existing characters and point action bar buttons that used it at the player Death Coil (rank 5).
UPDATE `character_action` SET `action` = 49895 WHERE `type` = 0 AND `action` IN (62900, 62901, 62902, 62903, 62904);
DELETE FROM `character_spell` WHERE `spell` IN (62900, 62901, 62902, 62903, 62904);
