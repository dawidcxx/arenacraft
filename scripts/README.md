# scripts

Development and operations helpers. Run the TypeScript files directly with Bun; each one
resolves its dependencies from `scripts/node_modules`.

```bash
cd scripts && bun install
```

## emit-fake-event

Publishes fake SoloQ events to Redis/Valkey so any listener (the Discord bot, custom tooling,
...) can be exercised without running the WoW core. It is standalone and does not import from
`apps/ac-bot`.

```bash
# full cycle (publish matchup, wait, publish ended)
bun run scripts/emit-fake-event.ts --channel both --instance 42 \
  --team-a ALICE,BOB,CAROL --team-b DAVE,ERIN,FRANK

# move real Discord users into the rooms (IDs from Discord, no DB lookup)
bun run scripts/emit-fake-event.ts --channel both --instance 42 \
  --team-a ALICE,BOB,CAROL --team-b DAVE,ERIN,FRANK \
  --team-a-ids 111111111111111111,222222222222222222,333333333333333333 \
  --team-b-ids 444444444444444444,555555555555555555,666666666666666666

# just a matchup
bun run scripts/emit-fake-event.ts --channel matchup --instance 42

# just an end (test crash recovery / unknown instance)
bun run scripts/emit-fake-event.ts --channel ended --instance 42
```

Options:

- `--channel matchup|ended|both` (default `both`)
- `--instance <id>` (default `9000`)
- `--delay <ms>` between the matchup and ended events (default `5000`)
- `--finished true|false`, `--winner teamA|teamB`
- `--team-a` / `--team-b` comma-separated account names
- `--team-a-ids` / `--team-b-ids` comma-separated Discord user IDs

Set `REDIS_URL` to target a non-default Redis/Valkey. Without `--team-a-ids` / `--team-b-ids`
a listener resolves the fake `accountId`s through `account_discord`; pass IDs for a purely fake
match with no DB accounts.
