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
              zig

              # native dependencies
              boost183
              readline
              bzip2
              zlib
              hiredis
              openssl
              mysql80

            ];
            MYSQL_INCLUDE_DIR = pkgs.mysql80 + "/include/mysql";
            shellHook = ''
              export PATH=$PATH:~/.local/arenacraft/bin
            '';
          };
        };
      }
    );
}
