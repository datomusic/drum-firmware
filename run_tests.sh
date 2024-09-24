#!/usr/bin/env bash
set -e

export PYTHONPATH=lib

mypy devices/teensy41
mypy firmware

pycodestyle firmware
pycodestyle devices

pytest
