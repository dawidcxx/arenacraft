-- Allow Soul Shards to stack (they ship unstackable in item_template).
UPDATE `item_template` SET `stackable` = 20 WHERE `entry` = 6265;
