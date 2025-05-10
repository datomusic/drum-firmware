#!/usr/bin/env bash

SOURCE_DIR="$1"

set -e

cd "$SOURCE_DIR"

echo "--------------------------"
echo "Executing tests: $SOURCE_DIR"
echo "--------------------------"
cmake -Bbuild -H. -DCMAKE_CXX_STANDARD=14 \
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
