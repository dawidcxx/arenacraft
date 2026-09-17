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
              zig
              zls
              pkg-config # zig uses this for .linkSystemLibrary()
              clang-tools
              lldb
              python3 # llms love python..

              # Libraries
              minizip
              zlib
              doctest
              jemalloc
              openssl
              hiredis
              bzip2
              readline
              ncurses
              # real libmysqlclient - mariadb-connector poisons
              # __cpp_nontype_template_args and lacks mysql_ssl_mode
              mysql84
            ];

            shellHook = ''
              unset NIX_CFLAGS_COMPILE
              export MYSQL_INCLUDE_DIR="${pkgs.mysql84}/include/mysql"
              export MYSQL_LIB_DIR="${pkgs.mysql84}/lib"
              export FLAKE_INCLUDES="${
                composeIncludePath [
                  pkgs.minizip
                  pkgs.zlib
                  pkgs.expat
                  pkgs.doctest
                  pkgs.hiredis
                ]
              }"
            '';
          };
        };
      }
    );
}
