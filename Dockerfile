# syntax=docker/dockerfile:1
# Two stages, both on the same Ubuntu LTS release (same glibc):
#   builder -> compiles the core with zig
#   runtime -> carries only the shared libraries the binary links
#
# Ubuntu (not Debian) is used because Oracle publishes no arm64 packages in its
# MySQL apt repo, while Debian only ships the mariadb connector - which this
# build rejects (it poisons __cpp_nontype_template_args and lacks
# mysql_ssl_mode). Ubuntu's own libmysqlclient-dev is the real MySQL 8.0 client
# and exists for amd64 and arm64 alike, so no third-party repo is needed and
# the image builds on either architecture.
#
# bun is deliberately absent: db_sync runs in the oven/bun image (see
# docker-compose.yml) and the cpp binary never shells out to it.

ARG UBUNTU_VERSION=22.04
ARG ZIG_VERSION=0.16.0

# --- builder ---------------------------------------------------------------
FROM ubuntu:${UBUNTU_VERSION} AS builder
ARG ZIG_VERSION

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl git xz-utils pkg-config \
        libssl-dev libhiredis-dev zlib1g-dev libbz2-dev libreadline-dev \
        libjemalloc-dev libmysqlclient-dev \
    && rm -rf /var/lib/apt/lists/*

# zig ships its own libc/libc++ headers and does not search /usr/include. The
# build asks pkg-config for each system library's include dirs and adds them
# as *system* include paths (zig-build/Deps.zig), so zig's own headers still
# take precedence. pkg-config hides system dirs by default - re-enable them.
# A few libraries need a stub: libbz2-dev ships no .pc at all, Ubuntu's
# hiredis.pc points at /usr/include/hiredis while the source includes
# <hiredis/hiredis.h>, and its openssl.pc omits the multiarch dir that holds
# opensslconf.h. libmysqlclient needs no stub - its paths are consumed
# directly by Deps.zig via MYSQL_INCLUDE_DIR / MYSQL_LIB_DIR.
ENV PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1 \
    MYSQL_INCLUDE_DIR=/usr/include/mysql \
    PKG_CONFIG_PATH=/opt/ac-pkgconfig

RUN mkdir -p /opt/ac-pkgconfig \
    && printf 'Name: bzip2\nVersion: 1.0.8\nDescription: bzip2\nLibs: -lbz2\nCflags: -I/usr/include\n' \
        > /opt/ac-pkgconfig/bzip2.pc \
    && printf 'Name: hiredis\nVersion: 1.0.0\nDescription: hiredis\nLibs: -lhiredis\nCflags: -I/usr/include\n' \
        > /opt/ac-pkgconfig/hiredis.pc \
    && MULTIARCH="$(uname -m)-linux-gnu" \
    && printf 'Name: openssl\nVersion: 3.0.0\nDescription: openssl\nLibs: -lssl -lcrypto\nCflags: -I/usr/include -I/usr/include/%s\n' \
        "$MULTIARCH" > /opt/ac-pkgconfig/openssl.pc

# zig bundles clang/lld/libc++, so no toolchain packages are needed. The
# tarball name uses the kernel arch (x86_64 / aarch64), same as uname -m.
RUN ZIG_ARCH="$(uname -m)" \
    && curl -fsSL "https://ziglang.org/download/${ZIG_VERSION}/zig-${ZIG_ARCH}-linux-${ZIG_VERSION}.tar.xz" \
        -o /tmp/zig.tar.xz \
    && tar -xf /tmp/zig.tar.xz -C /opt \
    && ln -s "/opt/zig-${ZIG_ARCH}-linux-${ZIG_VERSION}/zig" /usr/local/bin/zig \
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
    MYSQL_LIB_DIR="/usr/lib/$(uname -m)-linux-gnu" \
    zig build -Doptimize=ReleaseFast ac

# --- runtime ---------------------------------------------------------------
FROM ubuntu:${UBUNTU_VERSION} AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 libhiredis0.14 zlib1g libbz2-1.0 \
        libreadline8 libtinfo6 libjemalloc2 libmysqlclient21 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/zig-out/bin/ac /usr/local/bin/ac

WORKDIR /app
