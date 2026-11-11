-- DB update 2026_11_11_00 -> 2026_11_11_01
--
-- `.soloq details` help entry (src/game/Scripts/Commands/cs_soloq.cpp): console-level per-player queue view with the
-- effective MMR range.

DELETE FROM `command` WHERE `name` = 'soloq details';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
  ('soloq details', 4, 'Syntax: .soloq details\nList every queued player with class, spec, role, rating, MMR and effective MMR range.');
