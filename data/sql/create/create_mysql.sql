-- Idempotent development bootstrap: databases and the `acore` application user.
-- Applied by scripts/seed_db with root/admin credentials.

CREATE DATABASE IF NOT EXISTS `acore_world` DEFAULT CHARACTER SET UTF8MB4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS `acore_characters` DEFAULT CHARACTER SET UTF8MB4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS `acore_auth` DEFAULT CHARACTER SET UTF8MB4 COLLATE utf8mb4_unicode_ci;

CREATE USER IF NOT EXISTS 'acore'@'%' IDENTIFIED BY 'acore'
    WITH MAX_QUERIES_PER_HOUR 0 MAX_CONNECTIONS_PER_HOUR 0 MAX_UPDATES_PER_HOUR 0;
ALTER USER 'acore'@'%' IDENTIFIED BY 'acore';

GRANT ALL PRIVILEGES ON `acore_world`.* TO 'acore'@'%';
GRANT ALL PRIVILEGES ON `acore_characters`.* TO 'acore'@'%';
GRANT ALL PRIVILEGES ON `acore_auth`.* TO 'acore'@'%';
