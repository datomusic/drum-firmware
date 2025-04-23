#!/usr/bin/env bash

set -e


function build(){
  target_path="$1"
  echo ""
  echo "Building: $target_path"
  echo "-----"

  mkdir -p "build/$target_path"
  cmake -DPICO_BOARD=dato_submarine  -S "$target_path" -B "build/$target_path"
  pushd  "build/$target_path"
  make -j
  popd
}

build experiments/sample_player
build experiments/flash_rw
# build experiments/flash_audio_streaming
# build experiments/midi_cc
# build experiments/midi_sample_player
# build experiments/pizza_example
# build experiments/sine_test
# build experiments/flash_audio_streaming
# build experiments/keypad_example
# build experiments/midi_example/
