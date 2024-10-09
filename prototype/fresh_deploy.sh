#!/usr/bin/env bash

TARGET=~/mnt/CIRCUITPY

if [[ -d /Volumes/CIRCUITPY ]]; then
  TARGET=/Volumes/CIRCUITPY
fi

rm "$TARGET/code.py"
rm "$TARGET/settings.toml"
rm -r "${TARGET:?}/lib"
cp -r lib "$TARGET"
cp -r samples "$TARGET"
./transfer_firmware.sh
