-- Pounce Bleed (9007, 9824, 9826, 27007, 49804) is an NPC/triggered bleed, not a player ability; it is no longer
-- granted as a starting spell (src/game/Arenacraft/StartingSpells.cpp). Remove it from existing characters and clear
-- action bar buttons that used it.
DELETE FROM `character_action` WHERE `type` = 0 AND `action` IN (9007, 9824, 9826, 27007, 49804);
DELETE FROM `character_spell` WHERE `spell` IN (9007, 9824, 9826, 27007, 49804);
