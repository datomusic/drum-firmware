#!/usr/bin/env bash

cmake -Bbuild -H. -DCMAKE_CXX_STANDARD=14
pushd build && make && ctest --output-on-failure
