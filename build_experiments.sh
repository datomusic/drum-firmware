#!/usr/bin/env bash

set -e


function build_board(){
  board="$1"
  target_path="$2"
  echo ""
  echo "Building: $target_path [$board]"
  echo "-----"

  mkdir -p "build/$target_path"
  cmake -DPICO_BOARD="$board"  -S "$target_path" -B "build/$target_path"
  pushd  "build/$target_path"
  make -j
  popd
}


function build(){
  build_board dato_submarine "$1"
}

build_board pico2 experiments/midi_example/
build_board pico2  experiments/sine_test

build experiments/sample_player
build experiments/flash_rw
build experiments/midi_sample_player
# build experiments/flash_audio_streaming
# build experiments/midi_cc
# build experiments/pizza_example
# build experiments/keypad_example
