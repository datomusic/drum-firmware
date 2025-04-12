#!/usr/bin/env bash

function check(){
  find "$@" -regex '.*\.\(h\|c\|cpp\|hpp\|cc\|cxx\)' \
    -not -path '*build*' \
    -not -path '*.ccls-cache*' \
    -not -path './experiments/support/samples/*' \
    -not -path './experiments/pizza_example/*' \
    -not -path './experiments/midi_sample_player/*' \
    -not -path './experiments/keypad_example/*' \
    -not -path './experiments/drum_pad_test/*' \
    -exec clang-format --dry-run -Werror {} \;
}

check ./experiments
check ./drum
