#!/usr/bin/env bash
set -e

mypy devices/teensy41
mypy firmware

pycodestyle firmware
pycodestyle devices

pytest
