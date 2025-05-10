#!/usr/bin/env bash

set -e

echo "--------------------------"
echo "Executing tests"
echo "--------------------------"
cmake -DSTATIC_TESTS=0 -Bbuild -H. -DCMAKE_CXX_STANDARD=14 \
  && pushd build && make -j 8 \
  && ctest --output-on-failure

popd && rm -rf build

echo "------------------------"
echo "Building constexpr tests"
echo "------------------------"
cmake -DSTATIC_TESTS=1 -Bbuild -H. -DCMAKE_CXX_STANDARD=14 \
  && pushd build \
  && make -j 8
