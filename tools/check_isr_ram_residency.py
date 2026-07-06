#!/usr/bin/env python3
"""Verify the audio ISR call tree is RAM-resident in a firmware ELF.

The audio render runs in the I2S DMA interrupt, which stays enabled during
flash erase/program (musin/filesystem/audio_safe_flash.cpp). Any function
reachable from that interrupt must live in RAM: executing from flash (XIP)
while a sector is being erased hard-faults and watchdog-reboots the device.
See AGENTS.md, "Audio ISR RAM Residency".

Function-pointer edges (BufferSource vtables, audio_connection take/give)
cannot be traced statically, so every fill_buffer/read_samples implementation
and the buffer-pool/connection functions are treated as extra roots.

Usage: check_isr_ram_residency.py <firmware.elf> [--objdump PATH]
Exit status is non-zero if any reachable function has a flash address.
"""

import argparse
import re
import shutil
import subprocess
import sys
from collections import defaultdict

FLASH_BASE = 0x10000000
FLASH_END = 0x20000000

# Interrupt entry points and everything reachable only through function
# pointers. Matched as substrings of the demangled symbol name.
ROOT_PATTERNS = [
    "fill_buffers_from_irq",
    "audio_i2s_dma_irq_handler",
    "take_audio_buffer",
    "give_audio_buffer",
    "queue_full_audio_buffer",
    "queue_free_audio_buffer",
    "get_free_audio_buffer",
    "get_full_audio_buffer",
    "consumer_take",
    "producer_give",
    "::fill_buffer(",
    "::read_samples(",
]

FUNC_RE = re.compile(r"^([0-9a-f]{8}) <(.+)>:$")
BRANCH_RE = re.compile(
    r"\t(?:bl|blx|b|b\.n|b\.w|bl\.w|cbz|cbnz|"
    r"b(?:eq|ne|cs|cc|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)(?:\.n|\.w)?)"
    r"\t[0-9a-f]+ <([^>]+)>"
)


def disassemble(objdump, elf_path):
    result = subprocess.run(
        [objdump, "-dC", elf_path], capture_output=True, text=True, check=True
    )
    return result.stdout.splitlines()


def build_call_graph(lines):
    """Returns (address per function, callees per function)."""
    addresses = {}
    callees = defaultdict(set)
    current = None
    for line in lines:
        header = FUNC_RE.match(line)
        if header:
            current = header.group(2)
            addresses[current] = int(header.group(1), 16)
            continue
        if current is None:
            continue
        branch = BRANCH_RE.search(line)
        if branch:
            target = branch.group(1).split("+")[0]
            if target != current:
                callees[current].add(target)
    return addresses, callees


def reachable_from_roots(addresses, callees):
    # Veneers are flash-side trampolines for flash callers of RAM functions;
    # the ISR path calls the RAM address directly and never executes them.
    roots = {
        name
        for name in addresses
        if not name.endswith("_veneer")
        and any(pattern in name for pattern in ROOT_PATTERNS)
    }
    seen = set(roots)
    stack = list(roots)
    parent = {}
    while stack:
        name = stack.pop()
        for callee in callees[name]:
            if callee in addresses and callee not in seen:
                seen.add(callee)
                parent[callee] = name
                stack.append(callee)
    return roots, seen, parent


def call_path(name, parent):
    path = [name]
    while name in parent:
        name = parent[name]
        path.append(name)
    return " <- ".join(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("elf")
    parser.add_argument("--objdump", default=None)
    args = parser.parse_args()

    objdump = args.objdump or shutil.which("arm-none-eabi-objdump")
    if objdump is None:
        sys.exit("arm-none-eabi-objdump not on PATH; pass --objdump")

    addresses, callees = build_call_graph(disassemble(objdump, args.elf))
    roots, seen, parent = reachable_from_roots(addresses, callees)
    if not roots:
        sys.exit("No ISR root functions found; wrong ELF or symbol names changed")

    in_flash = lambda addr: FLASH_BASE <= addr < FLASH_END

    failures = [name for name in sorted(seen) if in_flash(addresses[name])]
    # A flash-side veneer for an ISR-tree function means some caller of it
    # still lives in flash; harmless by itself but worth surfacing.
    veneers = [
        name
        for name in sorted(addresses)
        if name.endswith("_veneer")
        and in_flash(addresses[name])
        and name.replace("__", "").removesuffix("_veneer") in seen
    ]

    print(f"Roots: {len(roots)}, reachable functions: {len(seen)}")
    if failures:
        print(f"\nFAIL: {len(failures)} reachable function(s) in flash:")
        for name in failures:
            print(f"  0x{addresses[name]:08x} {name}")
            print(f"    via: {call_path(name, parent)}")
        sys.exit(1)
    if veneers:
        print(f"\nNote: flash veneers exist for ISR-tree functions (flash "
              f"callers exist, ISR path itself is safe):")
        for name in veneers:
            print(f"  {name}")
    print("\nOK: entire ISR call tree is RAM-resident")


if __name__ == "__main__":
    main()
