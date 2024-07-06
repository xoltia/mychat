{ pkgs ? import <nixpkgs> { } }:
with pkgs;
mkShell {
  buildInputs = [
    ncurses
  ];

  nativeBuildInputs = [
    gcc
    pkg-config
  ];
}