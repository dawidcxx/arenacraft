{
  description = "Complete Open Source and Modular solution for MMO";

  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    {
      nixpkgs,
      flake-utils,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        devShells = {

          default = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } {
            nativeBuildInputs = with pkgs; [
              # dev tools
              zls
              bun
              clang-tools

              # build tools
              cmake
              ninja
              pkg-config
              zig_0_14

              # native dependencies
              boost183
              boost183.dev
              readline
              bzip2
              zlib
              hiredis
              openssl
              mysql80
              zstd
              lzlib
              xz

            ];
            MYSQL_INCLUDE_DIR = pkgs.mysql80 + "/include/mysql";
            shellHook = ''
              export PATH=$PATH:~/.local/arenacraft/bin
              export BOOST_PATH=${pkgs.boost183.dev}
              unset NIX_CFLAGS_COMPILE
              unset NIX_LDFLAGS
            '';
          };
        };
      }
    );
}
