# data/sql/updates

Forward-only migrations, applied by `ac db-update` (or
`zig build run-ac -- db-update`). To add one:

- drop a new `YYYY_MM_DD_NN.sql` into `db_auth/`, `db_characters/` or
  `db_world/`
- never edit or rename a file after it has been applied — db-update tracks
  applied files by sha1 and skips files whose hash changed
- there are no rollbacks/down-migrations; fix mistakes with a new file
- the full squashed schema lives in `../base/` (see
  `../base/database-squash.md`)
