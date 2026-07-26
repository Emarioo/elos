{ pkgs ? import <nixpkgs> {} }:

# This is not all dependencies you need on NixOS
# to build this project.

pkgs.mkShell {
  hardeningDisable = [ "fortify" ];

  buildInputs = with pkgs; [
    pkgsCross.mingwW64.buildPackages.gcc
    pkgsCross.mingwW64.buildPackages.binutils
    # It seems like NixOS has to build these, takes about 20-30 min
    # on my laptop which isn't great. Any alternative approach?
    # pkgsCross.x86_64-embedded.buildPackages.gcc
    # pkgsCross.x86_64-embedded.buildPackages.binutils
    
  ];
  # for some reason mkgpt puts the executable in
  # the root repo directory instead of a bin folder
  shellHook = ''
    export PATH="$PATH:${../mkgpt}"
  '';
}
