-- Solo 2v2 (src/game/Arenacraft/solo2v2, ai-doc/solo2v2.md): every character owns a single-member 2v2 arena team named
-- "<name>'s 2v2", starting at 1400 rating / 1500 MMR. Stock shared 2v2 teams are dropped (stock 2v2 no longer exists).
-- New characters get their team from C++ (character creation / login), this only backfills existing ones.

DELETE m FROM `arena_team_member` m INNER JOIN `arena_team` t ON t.`arenaTeamId` = m.`arenaTeamId` WHERE t.`type` = 2;
DELETE FROM `arena_team` WHERE `type` = 2;
DELETE FROM `character_arena_stats` WHERE `slot` = 0;

SET @base_id := (SELECT IFNULL(MAX(`arenaTeamId`), 0) FROM `arena_team`);

INSERT INTO `arena_team` (`arenaTeamId`, `name`, `captainGuid`, `type`, `rating`, `seasonGames`, `seasonWins`,
                          `weekGames`, `weekWins`, `rank`, `backgroundColor`, `emblemStyle`, `emblemColor`,
                          `borderStyle`, `borderColor`)
SELECT @base_id + ROW_NUMBER() OVER (ORDER BY c.`guid`), CONCAT(c.`name`, '''s 2v2'), c.`guid`, 2, 1400, 0, 0, 0, 0, 1,
       0, 0, 0, 0, 0
FROM `characters` c;

INSERT INTO `arena_team_member` (`arenaTeamId`, `guid`, `weekGames`, `weekWins`, `seasonGames`, `seasonWins`,
                                 `personalRating`)
SELECT t.`arenaTeamId`, t.`captainGuid`, 0, 0, 0, 0, 1400 FROM `arena_team` t WHERE t.`type` = 2;

INSERT INTO `character_arena_stats` (`guid`, `slot`, `matchMakerRating`, `maxMMR`)
SELECT c.`guid`, 0, 1500, 1500 FROM `characters` c;
