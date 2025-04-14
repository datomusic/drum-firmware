#!/usr/bin/env bash

set -e

if [[ "$1" == "check" ]]; then
  args='--dry-run'
else
  args='-i'
fi

function run(){
  root="$1"

  find "$root" -regex '.*\.\(h\|c\|cpp\|hpp\|cc\|cxx\)' \
    -not -path '*build*' \
    -not -path '*.ccls-cache*' \
    -not -path './musin_test/Catch2/*' \
    -not -path './musin/ports/pico/libraries/*' \
    -not -path './musin/ports/pico/pico-sdk/*' \
    -not -path './musin/ports/pico/pico-extras/*' \
    -not -path './musin/usb/*' \
    -not -path './musin/ui/*' \
    -not -path './musin/hal/*' \
    -not -path './musin/drivers/*' \
    -not -path './musin/boards/*' \
    -not -path './musin/audio/waveshaper.*' \
    -not -path './musin/audio/buffered_reader.h' \
    -not -path './musin/audio/audio_memory_reader.h' \
    -not -path './experiments/support/samples/*' \
    -not -path './experiments/pizza_example/*' \
    -not -path './experiments/midi_sample_player/*' \
    -not -path './experiments/drum_pad_test/*' \
    -exec clang-format -Werror "$args" {} +
}

function format(){
  run "$1" -i
}

function check(){
  run "$1" --dry-run
}

run ./experiments
run ./drum
run ./musin
run ./test/musin

if [ $? -eq 0 ]; then
  echo "All files are correctly formatted."
  exit 0
else
  exit 1
fi
