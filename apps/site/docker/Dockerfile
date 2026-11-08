FROM oven/bun:1-alpine AS builder

RUN apk add --no-cache git

WORKDIR /app

# Copy workspace configuration and lockfile
COPY package.json bun.lock ./

# Copy package manifests to resolve workspace dependencies
COPY ./packages/backend/package.json ./packages/backend/
COPY ./packages/web/package.json ./packages/web/
COPY ./packages/common/package.json ./packages/common/

# Install dependencies for the whole workspace
RUN bun install --frozen-lockfile

# Copy source files
COPY ./packages/backend ./packages/backend
COPY ./packages/web ./packages/web
COPY ./packages/common ./packages/common

# Build shared package, frontend and backend
# The site is mounted under /site on the host reverse proxy.
ENV VITE_BASE=/site/
RUN bun run build:common
RUN bun run build:web
RUN bun run build:backend

FROM oven/bun:1-alpine AS runtime
WORKDIR /app

# Backend runtime bundle (includes frontend under ./public when available)
COPY --from=builder /app/packages/backend/dist ./
ENV PORT=8080
ENV NODE_ENV=production
ENV DATA_SOURCE=mysql
ENV LOG_LEVEL=info

EXPOSE 8080

CMD ["bun", "run", "main.js"]
