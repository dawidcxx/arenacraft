-- Backfill existing characters' homebind to the arena hub in Netherstorm,
-- reported as Eco-Dome Midrealm (3877) so the Hearthstone tooltip stays generic.
-- New characters get this in code (Player::_LoadHomeBind).
REPLACE INTO `character_homebind` (`guid`, `mapId`, `zoneId`, `posX`, `posY`, `posZ`)
SELECT `guid`, 530, 3877, 3369.4014, 2882.7666, 143.8963 FROM `characters`;
