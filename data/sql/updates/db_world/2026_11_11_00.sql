-- DB update 2026_11_03_00 -> 2026_11_11_00
--
-- SoloQ command help entries (src/game/Scripts/Commands/cs_soloq.cpp). The command table drives `.help soloq` and the
-- subcommand list; previously none of the handlers had help text.

DELETE FROM `command` WHERE `name` IN
  ('soloq', 'soloq clear', 'soloq debug', 'soloq join', 'soloq list', 'soloq pending', 'soloq status');

INSERT INTO `command` (`name`, `security`, `help`) VALUES
  ('soloq', 0, 'Syntax: .soloq $subcommand\nType .soloq to see the list of possible subcommands or .help soloq $subcommand to see info on subcommands'),
  ('soloq clear', 3, 'Syntax: .soloq clear\nDequeue everyone and clear their queue badges.'),
  ('soloq debug', 3, 'Syntax: .soloq debug <on|off>\nToggle the character readiness checks (gear/enchants/gems/talents/glyphs) when queueing.'),
  ('soloq join', 0, 'Syntax: .soloq join\nJoin the SoloQ queue from anywhere (creates your team if needed).'),
  ('soloq list', 3, 'Syntax: .soloq list\nList every queued player with class, spec, role, rating, MMR and wait time.'),
  ('soloq pending', 3, 'Syntax: .soloq pending\nList the arenas waiting for a result (instance id and both teams).'),
  ('soloq status', 3, 'Syntax: .soloq status\nShow queued players by role, pending arenas, tourney mode and whether a match can form.');
