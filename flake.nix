{
  description = "Complete Open Source and Modular solution for MMO";

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
      in
      {
        devShells = {
          default = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } {
            nativeBuildInputs = with pkgs; [
              boost183
              cmake
              ninja
              openssl
              readline
              mysql80
              zlib
              pkg-config
              bzip2
              hiredis
              bun
              clang-tools
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
