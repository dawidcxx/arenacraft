# syntax=docker/dockerfile:1
# single-stage: build with the flake devshell, run in the same image - the
# binary's RPATHs point into /nix/store which is present at runtime
FROM nixos/nix:latest

# flake commands need the experimental features enabled
ENV NIX_CONFIG="extra-experimental-features = nix-command flakes"

WORKDIR /app

# warm the nix environment: cached until the flake changes
COPY flake.nix flake.lock ./
RUN nix develop -c true

# bun for db_sync (pinned via the flake's nixpkgs), on PATH for runtime
RUN ln -sf "$(nix develop -c sh -c 'command -v bun')" /nix/var/nix/profiles/default/bin/bun

# scripts deps first: node_modules layer survives src changes
COPY scripts/package.json scripts/bun.lock ./scripts/
RUN --mount=type=cache,target=/root/.bun bun install --production --frozen-lockfile --cwd scripts

# build inputs (see .dockerignore)
COPY build.zig build.zig.zon ./
COPY zig-build zig-build
COPY src src
COPY deps deps

RUN --mount=type=cache,target=/app/.zig-cache \
    --mount=type=cache,target=/app/zig-pkg \
    --mount=type=cache,target=/root/.cache/zig \
    nix develop -c zig build -Doptimize=ReleaseFast ac \
    && install -m755 zig-out/bin/ac /nix/var/nix/profiles/default/bin/ac

# sources for db_sync (bun install already done above)
COPY scripts/src ./scripts/src
