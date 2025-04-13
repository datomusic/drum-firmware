#!/usr/bin/env bash

cmake -Bbuild -H.
pushd build && make -j 8 && ctest --output-on-failure
