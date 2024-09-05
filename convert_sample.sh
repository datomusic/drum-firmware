#!/usr/bin/env bash

ffmpeg -i "$1" -bitexact -acodec pcm_s16le -ac 1 -map_metadata -1 -bitexact "$2"
