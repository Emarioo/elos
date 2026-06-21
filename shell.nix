{ pkgs ? import <nixpkgs> {} }:

# This is not all dependencies you need on NixOS
# to build this project.

pkgs.mkShell {
  buildInputs = [
    pkgs.pkgsCross.mingwW64.buildPackages.gcc
    pkgs.pkgsCross.mingwW64.buildPackages.binutils
    
  ];
  # for some reason mkgpt puts the executable in
  # the root repo directory instead of a bin folder
  shellHook = ''
    export PATH="$PATH:${../mkgpt}"
  '';
}
