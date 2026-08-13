#!/bin/bash

# Build script for the duo-on-drum milestone-1 app.
# Usage: duo/build.sh [-c] [-n]
#   -c  clean build directory first
#   -n  no upload (build only)
set -e

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" &> /dev/null && pwd)
pushd "$SCRIPT_DIR" > /dev/null
trap "popd > /dev/null" EXIT

CLEAN=false
UPLOAD=true

while getopts "cnh" opt; do
  case $opt in
    c) CLEAN=true ;;
    n) UPLOAD=false ;;
    h)
      echo "Usage: $0 [-c] [-n]"
      exit 0
      ;;
    *) exit 1 ;;
  esac
done

if [ "$CLEAN" = true ]; then
  rm -rf build
fi

GEN_ARGS=""
if command -v ninja >/dev/null 2>&1; then
  GEN_ARGS="-G Ninja"
fi

cmake -B build $GEN_ARGS -DCMAKE_BUILD_TYPE=Release -DPICO_COPY_TO_RAM=OFF
cmake --build build --parallel 10

UF2_FILE=$(find build -name "*.uf2" -print -quit)
echo "Built: $UF2_FILE"

if [ "$UPLOAD" = true ]; then
  if ! picotool info >/dev/null 2>&1; then
    echo "No device in BOOTSEL mode, attempting to force..."
    picotool info -F >/dev/null 2>&1 || {
      echo "Error: no RP2350 device found" >&2
      exit 1
    }
  fi
  picotool load -f "$UF2_FILE"
  picotool reboot
fi
