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
# keep in sync with flake.nix's zig (host-libc detection is 0.16 behaviour)
ARG ZIG_VERSION=0.16.0

# --- builder ---------------------------------------------------------------
FROM docker.io/library/ubuntu:${UBUNTU_VERSION} AS builder
ARG ZIG_VERSION

ENV DEBIAN_FRONTEND=noninteractive

# gcc is only here so zig can introspect the host libc: when link_libc is set
# and a native C compiler is present, zig uses the system libc (/usr/include
# and the multiarch dir). Without it zig silently falls back to its bundled
# libc, which does not search /usr/include. The compiler is never used to
# build anything.
# Third-party headers arrive through CPATH; host libc already covers
# /usr/include, so only mysql.h needs the extra /usr/include/mysql. bzip2
# comes from deps/bzip2 (Debian/Ubuntu ship no bzip2.pc); the final link finds
# libmysqlclient in the default multiarch lib dir, so no extra env is needed.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl git xz-utils pkg-config gcc \
        libssl-dev libhiredis-dev zlib1g-dev libreadline-dev \
        libjemalloc-dev libmysqlclient-dev \
    && rm -rf /var/lib/apt/lists/*

ENV CPATH=/usr/include/mysql

# zig bundles clang/lld/libc++, so gcc above is only used for libc detection,
# never as the build compiler. The tarball name uses the kernel arch
# (x86_64 / aarch64), same as uname -m.
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

# The three caches are owned by buildah/podman, not by the host checkout: the
# explicit ids give them a stable, project-scoped key so a nix host's
# .zig-cache can never be reused by this ubuntu build (and vice versa). On a
# podman builder they live under $TMPDIR/buildah-cache-$UID, so point TMPDIR at
# a persistent disk when building on an ephemeral/small machine.
RUN --mount=type=cache,id=arenacraft-zig-cache,target=/app/.zig-cache \
    --mount=type=cache,id=arenacraft-zig-pkg,target=/app/zig-pkg \
    --mount=type=cache,id=arenacraft-zig-global,target=/root/.cache/zig \
    zig build -Doptimize=ReleaseFast ac

# --- runtime ---------------------------------------------------------------
FROM docker.io/library/ubuntu:${UBUNTU_VERSION} AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 libhiredis0.14 zlib1g \
        libreadline8 libtinfo6 libjemalloc2 libmysqlclient21 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/zig-out/bin/ac /usr/local/bin/ac

WORKDIR /app
