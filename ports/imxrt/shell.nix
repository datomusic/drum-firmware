{pkgs ? import <nixpkgs> {}}:
  pkgs.mkShell {
    shellHook = ''
      export TOOLCHAIN_FILE=$(pwd)/cmake/toolchain/global-arm-gcc.cmake
    '';
    nativeBuildInputs = [
      pkgs.gcc-arm-embedded
    ];
  }
