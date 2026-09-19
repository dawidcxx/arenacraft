-- DB update 2024_11_15_00 -> 2026_09_19_00
--
-- Seed the administrator account `dawidgm`  with console
-- access on every realm.

INSERT INTO `account` (`username`, `salt`, `verifier`, `expansion`)
SELECT 'DAWIDGM',
       UNHEX('D116CF6F29B13840C5E1B5D3407C63AFDAD22BCC37AA79F51A0A58019384D7E7'),
       UNHEX('FE1642A6F7D078F015663629E6DF8B762CF3C5E51D3EFACAF13193C1A8D2986B'),
       2
WHERE NOT EXISTS (SELECT 1 FROM `account` WHERE `username` = 'DAWIDGM');

INSERT INTO `account_access` (`id`, `gmlevel`, `RealmID`, `comment`)
SELECT `id`, 3, -1, 'seeded administrator'
FROM `account`
WHERE `username` = 'DAWIDGM'
ON DUPLICATE KEY UPDATE `gmlevel` = 3, `comment` = 'seeded administrator';

-- Mirror LOGIN_INS_REALM_CHARACTERS_INIT so the account shows up on every realm.
INSERT INTO `realmcharacters` (`realmid`, `acctid`, `numchars`)
SELECT `r`.`id`, `a`.`id`, 0
FROM `realmlist` `r`
JOIN `account` `a` ON `a`.`username` = 'DAWIDGM'
LEFT JOIN `realmcharacters` `rc` ON `rc`.`realmid` = `r`.`id` AND `rc`.`acctid` = `a`.`id`
WHERE `rc`.`acctid` IS NULL;
