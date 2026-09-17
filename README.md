# Arenacraft

Custom mmo core based off [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk)

# Install

Dependencies: `nix` `docker` `wow-3.3.5a-client-files`

1. setup dev env
- `nix develop`
2. ensure `~/.local/arenacraft` exists
- `mkdir -p ~/.local/arenacraf`
3. setup 3rd party runtime dependencies
- `docker compose up`
4. build core
- `zig build ac`
5. drop client assets to `~/.local/var/wow`
6. extract client assets
- see OLD_SCRIPTS.md (extract-client, pending rewrite)
