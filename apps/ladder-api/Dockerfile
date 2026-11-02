# ladder-api: read-only public SoloQ ladder (see README.md). Built from apps/ladder-api.
FROM docker.io/oven/bun:1-alpine AS deps
WORKDIR /app
COPY package.json bun.lock ./
RUN bun install --production --frozen-lockfile

FROM docker.io/oven/bun:1-alpine
WORKDIR /app
ENV NODE_ENV=production
COPY --from=deps /app/node_modules ./node_modules
COPY package.json ./
COPY src ./src
# the image's unprivileged user; the filesystem is mounted read-only by docker-compose
USER bun
EXPOSE 8080
HEALTHCHECK --interval=10s --timeout=3s --retries=3 \
  CMD bun -e "fetch('http://127.0.0.1:8080/health').then(r => process.exit(r.ok ? 0 : 1)).catch(() => process.exit(1))"
CMD ["bun", "src/index.ts"]
