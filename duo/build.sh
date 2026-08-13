#!/bin/bash

# Build script for the duo-on-drum milestone-1 app.
# Usage: duo/build.sh [-c] [-n] [-p N]
#   -c    clean build directory first
#   -n    no upload (build only)
#   -p N  upload to partition N (0=A, 1=B); defaults to 0
#
# The partition matters. DUO and DRUM both declare binary version 1.0, so the
# bootrom has no way to prefer one over the other and boots partition 0. An
# upload with no partition is treated by picotool as an A/B update and lands
# in the inactive partition, where the image is never selected — the board
# keeps running whatever is in partition 0 and DUO looks like it failed to
# boot. Default to partition 0 so a plain upload actually runs.
set -e

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" &> /dev/null && pwd)
pushd "$SCRIPT_DIR" > /dev/null
trap "popd > /dev/null" EXIT

CLEAN=false
UPLOAD=true
PARTITION=0

while getopts "cnp:h" opt; do
  case $opt in
    c) CLEAN=true ;;
    n) UPLOAD=false ;;
    p) PARTITION="$OPTARG" ;;
    h)
      echo "Usage: $0 [-c] [-n] [-p N]"
      exit 0
      ;;
    *) exit 1 ;;
  esac
done

if ! [[ "$PARTITION" =~ ^[0-1]$ ]]; then
  echo "Error: partition must be 0 (Firmware A) or 1 (Firmware B)" >&2
  exit 1
fi

if [ "$CLEAN" = true ]; then
  rm -rf build
fi

GEN_ARGS=""
if command -v ninja >/dev/null 2>&1; then
  GEN_ARGS="-G Ninja"
fi

cmake -B build $GEN_ARGS -DCMAKE_BUILD_TYPE=Release -DPICO_COPY_TO_RAM=OFF
cmake --build build --parallel 10

# Pick the newest UF2: the build directory accumulates images from earlier
# builds under different version strings.
UF2_FILE=$(ls -t build/*.uf2 2>/dev/null \
  | grep -v -e '-direct\.uf2$' -e '/partition_table\.uf2$' | head -1)
echo "Built: $UF2_FILE"

if [ "$UPLOAD" = true ]; then
  if ! picotool info >/dev/null 2>&1; then
    echo "No device in BOOTSEL mode, attempting to force..."
    picotool info -F >/dev/null 2>&1 || {
      echo "Error: no RP2350 device found" >&2
      exit 1
    }
  fi
  if ! picotool partition info -f 2>/dev/null \
      | grep -q "^[[:space:]]*$PARTITION("; then
    echo "Error: partition $PARTITION not found on the connected device" >&2
    echo "Available partitions:" >&2
    picotool partition info -f 2>/dev/null | grep "^[[:space:]]*[0-9](" >&2
    exit 1
  fi

  echo "Uploading to partition $PARTITION..."
  picotool load -f -p "$PARTITION" "$UF2_FILE"
  picotool reboot -f -g "$PARTITION"
fi
