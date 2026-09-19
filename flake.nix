{

  description = "Basic C++ project using Zig as build system with clangd support";

  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
        lib = nixpkgs.lib;
        composeLibraryPath =
          packages: lib.concatStringsSep ":" (map (pkg: "${pkg.out or pkg}/lib") packages);
        composeIncludePath =
          packages: lib.concatStringsSep ":" (map (pkg: "${pkg.dev or pkg}/include") packages);
      in
      {
        devShells = {
          default = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } {
            nativeBuildInputs = with pkgs; [
              # Dev tools
              zig_0_16 # keep in sync with Dockerfile ZIG_VERSION (libc detection)
              zls
              pkg-config # zig uses this for .linkSystemLibrary()
              clang-tools
              lldb
              python3 # llms love python..
              bun # scripts tooling (db_sync, extract_assets)
              gdb
              netcat

              # Libraries
              zlib
              doctest
              jemalloc
              openssl
              hiredis
              readline
              ncurses
              mysql84
            ];

            shellHook = ''
              unset NIX_CFLAGS_COMPILE
              # Standard compiler env vars, honoured by zig natively, so the
              # build graph carries no pkg-config include/link plumbing:
              # CPATH for headers (mysql.h sits in include/mysql, not include)
              # and LIBRARY_PATH for the final link (mysqlclient has no .pc).
              export CPATH="${
                composeIncludePath [
                  pkgs.openssl
                  pkgs.hiredis
                  pkgs.zlib
                  pkgs.readline
                  pkgs.jemalloc
                ]
              }:${pkgs.mysql84}/include/mysql"
              export LIBRARY_PATH="${
                composeLibraryPath [
                  pkgs.openssl
                  pkgs.hiredis
                  pkgs.zlib
                  pkgs.readline
                  pkgs.jemalloc
                  pkgs.mysql84
                ]
              }"
            '';
          };
        };
      }
    );
}
