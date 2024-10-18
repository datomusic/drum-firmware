#!/usr/bin/env bash


# Write NXP's Flashloader to RAM using the built in USB bootloader
sdphost -u 0x1FC9,0x0145 -- write-file 0x20205800 ./ivt_flashloader.bin
# Then jump to the address where the Flashloader was placed
sdphost -u 0x1FC9,0x0145 -- jump-address 0x20205800
