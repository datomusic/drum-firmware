#!/usr/bin/env bash

SOURCE_DIR="$1"

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# If SOURCE_DIR is not an absolute path, make it relative to the script directory
if [[ "$SOURCE_DIR" != /* ]]; then
  SOURCE_DIR="$SCRIPT_DIR/$SOURCE_DIR"
fi

cd "$SOURCE_DIR"

echo "--------------------------"
echo "Executing tests: $SOURCE_DIR"
echo "--------------------------"
cmake -Bbuild -DSTATIC_TESTS=0 -H. -DCMAKE_CXX_STANDARD=14 \
  && pushd build && make -j 8 \
  && ctest --output-on-failure\
  && popd \
  && rm -rf build \
  && echo "------------------------" \
  && echo "Building constexpr tests: $SOURCE_DIR" \
  && echo "------------------------" \
  && cmake -DSTATIC_TESTS=1 -Bbuild -H. -DCMAKE_CXX_STANDARD=14 \
  && pushd build \
  && make -j 8
