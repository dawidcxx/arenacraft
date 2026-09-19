-- DB update 2026_09_19_00 -> 2026_09_19_01
--
-- Rename the default realm "AzerothCore" (the row seeded by
-- data/sql/base/db_auth/realmlist.sql) to "Arenacraft" and expose it on all
-- interfaces by advertising 0.0.0.0 instead of the loopback interface.
-- Both the external endpoint (`address`) and the local one (`localAddress`)
-- are switched; `localSubnetMask` is left untouched.
--
-- Note: the world server's actual listen interface is the `BindIP` env option
-- (already defaults to 0.0.0.0); this row is only what the auth server hands to
-- clients in the realm list.

UPDATE `realmlist`
SET `name` = 'Arenacraft',
    `address` = '0.0.0.0',
    `localAddress` = '0.0.0.0'
WHERE `id` = 1;
