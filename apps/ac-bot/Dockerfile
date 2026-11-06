# ac-bot: Arenacraft Discord bot (see README.md). Built from apps/ac-bot.

# --- build ---------------------------------------------------------------------------------------
FROM docker.io/oven/bun:1-alpine AS build
WORKDIR /app

COPY package.json bun.lock ./
RUN bun install --frozen-lockfile --production

COPY src ./src

# Self-contained executable: the runtime image gets one file, not node_modules + source.
RUN bun build --compile --minify --outfile /app/ac-bot src/index.ts

# --- runtime -------------------------------------------------------------------------------------
FROM docker.io/alpine:3.22 AS runtime
WORKDIR /app

# Bun's standalone executable needs libstdc++ on musl.
RUN apk add --no-cache libstdc++

COPY --from=build /app/ac-bot ./ac-bot

# Unprivileged user; the bot only needs outbound connections (Discord, MySQL, Redis).
RUN addgroup -S app && adduser -S -G app app
USER app

ENTRYPOINT ["./ac-bot"]
