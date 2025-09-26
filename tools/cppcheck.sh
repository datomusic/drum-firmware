#!/bin/bash
# cppcheck runner for DRUM firmware
# Usage: ./tools/cppcheck.sh [directory]

TARGET_DIR=${1:-"drum/"}

echo "Running cppcheck on $TARGET_DIR..."

cppcheck \
  --enable=all \
  --std=c++20 \
  --platform=unix32 \
  --inconclusive \
  --quiet \
  --template='{file}:{line}: {severity}: {message}' \
  --suppress=missingIncludeSystem \
  --suppress=missingInclude \
  --suppress=unusedFunction \
  --suppress=unmatchedSuppression \
  --suppress=varFuncNullUB \
  --suppress='*:*/pico-sdk/*' \
  --suppress='*:*/build/*' \
  --suppress='*:*/.pio/*' \
  --suppress='cstyleCast:*/hardware/*' \
  --suppress='variableScope:*/hardware/*' \
  --suppress='knownConditionTrueFalse:*/hardware/*' \
  --suppress=shiftTooManyBits \
  --suppress=ConfigurationNotChecked \
  -I . \
  -I drum/ \
  -I musin/ \
  "$TARGET_DIR"
