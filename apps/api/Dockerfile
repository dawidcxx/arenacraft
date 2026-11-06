# api: read-only public SoloQ ladder (see README.md). Built from apps/api.

# --- build ---------------------------------------------------------------------------------------
FROM docker.io/oven/bun:1-alpine AS build
WORKDIR /app

COPY package.json bun.lock ./
RUN bun install --frozen-lockfile --production

COPY src ./src

# Self-contained executable: the runtime image gets one file, not node_modules + source.
RUN bun build --compile --minify --outfile /app/api src/index.ts

# --- runtime -------------------------------------------------------------------------------------
FROM docker.io/alpine:3.22 AS runtime
WORKDIR /app

# Bun's standalone executable needs libstdc++ on musl.
RUN apk add --no-cache libstdc++

COPY --from=build /app/api ./api

# Unprivileged user; the filesystem is mounted read-only by docker-compose/production.
RUN addgroup -S app && adduser -S -G app app
USER app

EXPOSE 8080

HEALTHCHECK --interval=10s --timeout=3s --retries=3 \
  CMD wget -q -O /dev/null http://127.0.0.1:8080/health || exit 1

CMD ["./api"]
