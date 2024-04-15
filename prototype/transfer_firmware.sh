#!/usr/bin/env bash

TARGET=~/mnt/CIRCUITPY
DEVICE_NAME=teensy41


function send() {
  from=$1
  to=$2
  rsync -rz --progress --update \
    --exclude=__pycache__ \
    --exclude=.mypy_cache \
    --exclude="*.pyc" \
    "$from" "$to"
}

send firmware "$TARGET"
send devices/$DEVICE_NAME "$TARGET"
send devices/$DEVICE_NAME/code.py "$TARGET"
