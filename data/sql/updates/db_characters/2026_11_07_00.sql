-- Greater Blessing of Sanctuary (25899) is no longer granted as a starting spell
-- (src/game/Arenacraft/StartingSpells.cpp); remove it from existing characters and clear action bar buttons that used it.
DELETE FROM `character_action` WHERE `type` = 0 AND `action` = 25899;
DELETE FROM `character_spell` WHERE `spell` = 25899;
