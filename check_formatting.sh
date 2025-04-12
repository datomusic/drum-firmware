#!/usr/bin/env bash

function check(){
  find "$@" -regex '.*\.\(h\|c\|cpp\|hpp\|cc\|cxx\)' \
    -not -path '*build*' \
    -not -path '*.ccls-cache*' \
    -not -path './musin/ports/pico/libraries/*' \
    -not -path './musin/ports/pico/pico-sdk/*' \
    -not -path './musin/ports/pico/pico-extras/*' \
    -not -path './musin/usb/*' \
    -not -path './musin/ui/*' \
    -not -path './musin/ui/*' \
    -not -path './musin/hal/*' \
    -not -path './musin/drivers/*' \
    -not -path './musin/boards/*' \
    -not -path './musin/audio/waveshaper.*' \
    -not -path './experiments/support/samples/*' \
    -not -path './experiments/pizza_example/*' \
    -not -path './experiments/midi_sample_player/*' \
    -not -path './experiments/keypad_example/*' \
    -not -path './experiments/drum_pad_test/*' \
    -exec clang-format --dry-run -Werror {} \;
}

check ./experiments
check ./drum
# check ./musin
