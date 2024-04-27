#!/usr/bin/env bash

TARGET=~/mnt/CIRCUITPY

if [[ -d /Volumes/CIRCUITPY ]]; then
  TARGET=/Volumes/CIRCUITPY
fi

DEVICE_NAME=teensy41

echo "Copying firmware $DEVICE_NAME to $TARGET"

function send() {
  from=$1
  to=$2
  rsync -rz --progress --update \
    --bwlimit=100 \
    --exclude=__pycache__ \
    --exclude=.mypy_cache \
    --exclude="*.pyc" \
    "$from" "$to"
}

send firmware "$TARGET"
send devices/$DEVICE_NAME "$TARGET"
send devices/$DEVICE_NAME/code.py "$TARGET"
