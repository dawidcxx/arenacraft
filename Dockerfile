# syntax=docker/dockerfile:1
# multi-stage: build in debian-slim with zig + system dev libs, run in a
# slim runtime image carrying only the shared libs the binary links.
# builder and runtime must stay on the same debian release (same glibc).
# bun is intentionally absent: db_sync runs in the oven/bun image directly
# (see docker-compose.yml), and the cpp binary never shells out to it.

ARG DEBIAN_VERSION=docker.io/library/debian:bookworm-slim
ARG ZIG_VERSION=0.16.0

# --- builder ---------------------------------------------------------------
FROM ${DEBIAN_VERSION} AS builder
ARG ZIG_VERSION

# system build deps; the mysql client comes from Oracle's repo because
# Debian only ships mariadb, and mariadb-connector is rejected by the build
# (poisons __cpp_nontype_template_args, lacks mysql_ssl_mode)
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl xz-utils git gnupg pkg-config \
        libssl-dev libhiredis-dev zlib1g-dev libbz2-dev libreadline-dev \
        libjemalloc-dev \
    && curl -fsSL https://repo.mysql.com/RPM-GPG-KEY-mysql-2025 \
        | gpg --dearmor -o /usr/share/keyrings/mysql-2025.gpg \
    && curl -fsSL https://repo.mysql.com/RPM-GPG-KEY-mysql-2023 \
        | gpg --dearmor -o /usr/share/keyrings/mysql-2023.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/mysql-2025.gpg,/usr/share/keyrings/mysql-2023.gpg] http://repo.mysql.com/apt/debian bookworm mysql-8.0" \
        > /etc/apt/sources.list.d/mysql.list \
    && apt-get update && apt-get install -y --no-install-recommends libmysqlclient-dev \
    && rm -rf /var/lib/apt/lists/*

# mirrors the flake's shellHook exports (no pkg-config file for mysqlclient);
# Oracle's deb drops libmysqlclient straight into the multiarch lib dir
ENV MYSQL_INCLUDE_DIR=/usr/include/mysql \
    MYSQL_LIB_DIR=/usr/lib/x86_64-linux-gnu \
    PKG_CONFIG_PATH=/opt/ac-pkgconfig

# zig's bundled glibc/libc++ headers must own the standard-header namespace
# (mixing in /usr/include breaks them), so the system headers are exposed
# per-library through an isolated include dir + shim .pc files that pkgconf
# resolves ahead of debian's (which would otherwise emit nothing or -I/usr/include)
RUN mkdir -p /opt/ac-include /opt/ac-pkgconfig /opt/ac-include/openssl \
    && ln -s /usr/include/zlib.h /usr/include/zconf.h /usr/include/bzlib.h /opt/ac-include/ \
    && ln -s /usr/include/openssl/* /opt/ac-include/openssl/ \
    && ln -s /usr/include/x86_64-linux-gnu/openssl/* /opt/ac-include/openssl/ \
    && ln -s /usr/include/hiredis /usr/include/readline /opt/ac-include/ \
    && for spec in "zlib:-lz" "openssl:-lssl -lcrypto" "hiredis:-lhiredis" "bzip2:-lbz2" "readline:-lreadline"; do \
        name="${spec%%:*}"; libs="${spec#*:}"; \
        printf 'Name: %s\nVersion: 1.0\nDescription: %s (container shim)\nLibs: -L/usr/lib/x86_64-linux-gnu %s\nCflags: -I/opt/ac-include\n' \
            "$name" "$name" "$libs" > "/opt/ac-pkgconfig/$name.pc"; \
    done

# zig ships its own clang/lld/libc++: the whole C++ side compiles with it,
# no toolchain packages needed
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
FROM ${DEBIAN_VERSION} AS runtime

# shared libs linked by ac; the mysql client runtime must match the builder's
# repo exactly (libmysqlclient ABI)
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl gnupg \
        libssl3 libhiredis0.14 zlib1g libbz2-1.0 \
        libreadline8 libtinfo6 libjemalloc2 \
    && curl -fsSL https://repo.mysql.com/RPM-GPG-KEY-mysql-2025 \
        | gpg --dearmor -o /usr/share/keyrings/mysql-2025.gpg \
    && curl -fsSL https://repo.mysql.com/RPM-GPG-KEY-mysql-2023 \
        | gpg --dearmor -o /usr/share/keyrings/mysql-2023.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/mysql-2025.gpg,/usr/share/keyrings/mysql-2023.gpg] http://repo.mysql.com/apt/debian bookworm mysql-8.0" \
        > /etc/apt/sources.list.d/mysql.list \
    && apt-get update && apt-get install -y --no-install-recommends libmysqlclient21 \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get purge -y --auto-remove curl gnupg

COPY --from=builder /app/zig-out/bin/ac /usr/local/bin/ac

WORKDIR /app
