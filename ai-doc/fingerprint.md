# Fingerprints

GM tool to recognise a known cheater's setup on other accounts. Code: `src/game/Arenacraft/fingerprint/`
(`Fingerprint` = pure parsing and scoring + tests, `FingerprintScripts` = commands and storage). Tables (characters
db, `data/sql/updates/db_characters/2026_11_03_00.sql`): `fingerprint`, `account_addon_list`.

## What a fingerprint is

Only data the 3.3.5 client itself keeps on the server - nothing is read from the player's machine:

- **Macros** - `macros-cache` account data (account-wide `account_data` type 4 + per character
  `character_account_data` type 5). The client uploads it whenever a macro changes.
- **Key bindings** - `bindings-cache` (types 2 and 3).
- **Addons** - the list the client sends with every login (`WorldSession::ReadAddonsInfo`); the enabled ones are kept
  per account in `account_addon_list`, so it is available while the player is offline. Blizzard_ addons are ignored.

## Commands (game masters)

- `.fingerprint touch <name> [reason]` - store the player's current fingerprint (online or offline).
- `.fingerprint compare <name>` - score the player against every stored fingerprint of other accounts, print the 5
  closest: overall % and macros / addons / bindings %.
- `.fingerprint list` - stored fingerprints. `.fingerprint delete <id>` (administrators).

## Scoring

- Macros: the set of macro body lines (lowercased, whitespace collapsed). Bindings: the set of `key=command`. Addons:
  the set of names.
- Each part is a weighted Jaccard similarity. An entry's weight is `1 + ln((N + 1) / (df + 1))` over the stored
  fingerprints plus the compared player (N fingerprints, df = how many have the entry): stock bindings or popular
  addons everybody has barely count, an unusual macro line or addon dominates.
- Overall = weighted mean of the parts both sides have: macros 45%, addons 35%, bindings 20%. A part only one side has
  counts as 0%; a part neither side has is left out.

## Limits

- The client only syncs macros/bindings while it runs with settings sync on (the default); a fresh account may have
  nothing yet.
- A high score is a lead for a human, not proof: players copy macros and addon packs from each other.
- Renaming an addon folder or rewriting macros defeats it; it catches lazy reuse of a cheat setup.
