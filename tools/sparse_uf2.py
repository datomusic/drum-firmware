#!/usr/bin/env python3
"""Strip erased-flash (all-0xFF) blocks from a UF2 image.

A full-flash dump from `picotool save` includes every erased sector. The
RP2350 bootrom erases each sector before the first write that touches it,
so blocks whose payload is entirely 0xFF can be dropped: written sectors
still end up with 0xFF in the skipped gaps, and untouched sectors stay
erased on factory-blank flash.

Note: the resulting sparse image only programs the blocks it contains. On
a previously used device, regions the image skips entirely keep their old
contents. Intended for factory flashing of blank devices.

Usage: sparse_uf2.py input.uf2 [output.uf2]
"""

import struct
import sys

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_BLOCK_SIZE = 512


def strip_empty_blocks(data: bytes) -> bytes:
  if len(data) % UF2_BLOCK_SIZE != 0:
    raise ValueError("input size is not a multiple of 512 bytes")

  kept = []
  for offset in range(0, len(data), UF2_BLOCK_SIZE):
    block = data[offset:offset + UF2_BLOCK_SIZE]
    magic0, magic1 = struct.unpack_from("<II", block, 0)
    if (magic0, magic1) != (UF2_MAGIC_START0, UF2_MAGIC_START1):
      raise ValueError(f"bad UF2 magic at offset {offset:#x}")
    payload_size, = struct.unpack_from("<I", block, 16)
    payload = block[32:32 + payload_size]
    if payload.count(0xFF) != payload_size:
      kept.append(bytearray(block))

  for index, block in enumerate(kept):
    struct.pack_into("<II", block, 20, index, len(kept))

  return b"".join(kept)


def main() -> int:
  if len(sys.argv) not in (2, 3):
    print(__doc__.strip(), file=sys.stderr)
    return 1

  src = sys.argv[1]
  dst = sys.argv[2] if len(sys.argv) == 3 else src.removesuffix(".uf2") + "-sparse.uf2"

  with open(src, "rb") as f:
    data = f.read()
  sparse = strip_empty_blocks(data)
  with open(dst, "wb") as f:
    f.write(sparse)

  total = len(data) // UF2_BLOCK_SIZE
  kept = len(sparse) // UF2_BLOCK_SIZE
  print(f"{src}: {total} blocks -> {kept} blocks "
        f"({len(sparse) / 1e6:.1f} MB, dropped {total - kept} empty)")
  print(f"wrote {dst}")
  return 0


if __name__ == "__main__":
  sys.exit(main())
