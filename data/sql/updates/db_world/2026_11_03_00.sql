-- Starfall (#45): the splash (50294 and ranks) hits the star's main target too. The script that filtered the
-- explicit target out of the splash (spell_dru_starfall_aoe) was removed from the core.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dru_starfall_aoe';
