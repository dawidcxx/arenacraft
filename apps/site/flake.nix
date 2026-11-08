{
  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    inputs:
    inputs.flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = (import (inputs.nixpkgs) { inherit system; });
        script = name: pkgs.writeShellScriptBin name (builtins.readFile ./scripts/nix/${name});
      in
      {
        devShell = pkgs.mkShell {
          buildInputs =
            (with pkgs; [
              git
              zip

              # web dev tools
              bun
              oxlint
              oxfmt

              # nix related
              nixpkgs-fmt
            ])
            ++ (map script [
              "format"
              "format-check"
              "lint"
              "build"
              "run"
              "clean"
              "build-oci-images"
            ]);
        };

      }
    );
}
