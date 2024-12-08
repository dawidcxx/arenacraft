{
  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = inputs:
    inputs.flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = (import (inputs.nixpkgs) { inherit system; });
      in
      {
        devShell = pkgs.mkShell {
          buildInputs = with pkgs; [
            git
            zip
            bashInteractive

            rustc
            cargo
            nodejs_22
            pnpm
            
            pkg-config
            openssl
            wrapGAppsHook4
            glib-networking
            webkitgtk_4_1

            cmake
            zlib
            bzip2

            # nix related
            nixpkgs-fmt
          ];
        };
        
      }
    );
}
