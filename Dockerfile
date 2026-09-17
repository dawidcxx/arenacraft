# syntax=docker/dockerfile:1
# Two stages, both on the same Debian release (same glibc):
#   builder -> compiles the core with zig
#   runtime -> carries only the shared libraries the binary links
#
# The mysql client comes from Oracle's apt repo because Debian ships mariadb,
# and the mariadb connector is rejected by the build. Both stages install from
# that repo, so the keyring/repo setup lives in a shared base stage.
#
# bun is deliberately absent: db_sync runs in the oven/bun image (see
# docker-compose.yml) and the cpp binary never shells out to it.

ARG DEBIAN_VERSION=docker.io/library/debian:bookworm-slim
ARG ZIG_VERSION=0.16.0

# --- shared base: Oracle mysql apt repo ------------------------------------
FROM ${DEBIAN_VERSION} AS base

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl gnupg \
    && curl -fsSL https://repo.mysql.com/RPM-GPG-KEY-mysql-2025 \
        | gpg --dearmor -o /usr/share/keyrings/mysql-2025.gpg \
    && curl -fsSL https://repo.mysql.com/RPM-GPG-KEY-mysql-2023 \
        | gpg --dearmor -o /usr/share/keyrings/mysql-2023.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/mysql-2025.gpg,/usr/share/keyrings/mysql-2023.gpg] http://repo.mysql.com/apt/debian bookworm mysql-8.0" \
        > /etc/apt/sources.list.d/mysql.list \
    && rm -rf /var/lib/apt/lists/*

# --- builder ---------------------------------------------------------------
FROM base AS builder
ARG ZIG_VERSION

RUN apt-get update && apt-get install -y --no-install-recommends \
        git xz-utils pkg-config \
        libssl-dev libhiredis-dev zlib1g-dev libbz2-dev libreadline-dev \
        libjemalloc-dev libmysqlclient-dev \
    && rm -rf /var/lib/apt/lists/*

# zig ships its own libc/libc++ headers and does not search /usr/include. The
# build asks pkg-config for each system library's include dirs and adds them
# as *system* include paths (zig-build/Deps.zig), so zig's own headers still
# take precedence. pkg-config hides system dirs by default - re-enable them.
# A few libraries need a stub: libbz2-dev ships no .pc at all, Debian's
# hiredis.pc points at /usr/include/hiredis while the source includes
# <hiredis/hiredis.h>, and its openssl.pc omits the multiarch dir that holds
# opensslconf.h. Oracle's libmysqlclient needs no stub - its paths are
# consumed directly by Deps.zig.
ENV PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1 \
    MYSQL_INCLUDE_DIR=/usr/include/mysql \
    MYSQL_LIB_DIR=/usr/lib/x86_64-linux-gnu \
    PKG_CONFIG_PATH=/opt/ac-pkgconfig

RUN mkdir -p /opt/ac-pkgconfig \
    && printf 'Name: bzip2\nVersion: 1.0.8\nDescription: bzip2\nLibs: -lbz2\nCflags: -I/usr/include\n' \
        > /opt/ac-pkgconfig/bzip2.pc \
    && printf 'Name: hiredis\nVersion: 1.0.0\nDescription: hiredis\nLibs: -lhiredis\nCflags: -I/usr/include\n' \
        > /opt/ac-pkgconfig/hiredis.pc \
    && printf 'Name: openssl\nVersion: 3.0.0\nDescription: openssl\nLibs: -lssl -lcrypto\nCflags: -I/usr/include -I/usr/include/x86_64-linux-gnu\n' \
        > /opt/ac-pkgconfig/openssl.pc

# zig bundles clang/lld/libc++, so no toolchain packages are needed
RUN curl -fsSL "https://ziglang.org/download/${ZIG_VERSION}/zig-x86_64-linux-${ZIG_VERSION}.tar.xz" \
        -o /tmp/zig.tar.xz \
    && tar -xf /tmp/zig.tar.xz -C /opt \
    && ln -s "/opt/zig-x86_64-linux-${ZIG_VERSION}/zig" /usr/local/bin/zig \
    && rm /tmp/zig.tar.xz

WORKDIR /app

# build inputs (see .dockerignore); boost is fetched by zig itself, so the
# zig-pkg cache mount below keeps that layer warm across src changes
COPY build.zig build.zig.zon ./
COPY zig-build zig-build
COPY src src
COPY deps deps

RUN --mount=type=cache,target=/app/.zig-cache \
    --mount=type=cache,target=/app/zig-pkg \
    --mount=type=cache,target=/root/.cache/zig \
    zig build -Doptimize=ReleaseFast ac

# --- runtime ---------------------------------------------------------------
FROM base AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 libhiredis0.14 zlib1g libbz2-1.0 \
        libreadline8 libtinfo6 libjemalloc2 libmysqlclient21 \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get purge -y --auto-remove curl gnupg

COPY --from=builder /app/zig-out/bin/ac /usr/local/bin/ac

WORKDIR /app
