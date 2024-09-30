#!/usr/bin/env bash

TARGET=~/mnt/CIRCUITPY

if [[ -d /Volumes/CIRCUITPY ]]; then
  TARGET=/Volumes/CIRCUITPY
fi

rm "$TARGET/code.py"
rm "$TARGET/settings.toml"
rmdir "$TARGET/lib"
cp -r lib "$TARGET"
./transfer_firmware.sh
