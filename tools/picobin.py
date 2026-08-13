"""Shared picobin/UF2 helpers.

Used by both `make_factory_uf2.py` (which composes a pre-bought factory
image) and `clear_tbyb_uf2.py` (which pre-buys a developer build so it can
be flashed directly). The TBYB definition lives here so the two callers
cannot drift apart.

Deliberately dependency-free: `clear_tbyb_uf2.py` runs under a bare
`python3` from `drum/build.sh`, with no `uv` and no PEP 723 environment.
"""

import struct

FLASH_BASE = 0x10000000
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_BLOCK_SIZE = 512
UF2_HEADER_SIZE = 32

PICOBIN_BLOCK_MARKER_START = 0xFFFFDED3
PICOBIN_ITEM_TYPE_IMAGE_TYPE = 0x42
PICOBIN_IMAGE_TYPE_EXE_TBYB_BITS = 0x8000


def clear_tbyb(firmware):
  """Clear the try-before-you-buy flag in the picobin IMAGE_TYPE item.

  The build sets TBYB so A/B updates boot as flash-update trials, but the
  bootrom refuses to boot an unbought TBYB image on a normal power-on. A
  directly-flashed image must be pre-bought — the same state
  rom_explicit_buy leaves in flash after a successful update.

  `firmware` is a mutable flash-relative image; patched in place."""
  cleared = 0
  marker = struct.pack("<I", PICOBIN_BLOCK_MARKER_START)
  pos = firmware.find(marker)
  while pos >= 0:
    item_offset = pos + 4
    if firmware[item_offset] == PICOBIN_ITEM_TYPE_IMAGE_TYPE:
      flags, = struct.unpack_from("<H", firmware, item_offset + 2)
      if flags & PICOBIN_IMAGE_TYPE_EXE_TBYB_BITS:
        struct.pack_into("<H", firmware, item_offset + 2,
                         flags & ~PICOBIN_IMAGE_TYPE_EXE_TBYB_BITS)
        cleared += 1
    pos = firmware.find(marker, pos + 4)
  if cleared != 1:
    raise ValueError(f"expected exactly one TBYB IMAGE_TYPE item, "
                     f"patched {cleared}")


def uf2_blocks(data):
  """Yield (file_offset, address, payload_size) for each UF2 block."""
  if len(data) % UF2_BLOCK_SIZE:
    raise ValueError(f"UF2 length {len(data)} is not a multiple of "
                     f"{UF2_BLOCK_SIZE}")
  for offset in range(0, len(data), UF2_BLOCK_SIZE):
    magic0, magic1, _flags, addr, size = struct.unpack_from("<5I", data, offset)
    if (magic0, magic1) != (UF2_MAGIC_START0, UF2_MAGIC_START1):
      raise ValueError(f"bad UF2 magic at offset {offset:#x}")
    if size > UF2_BLOCK_SIZE - UF2_HEADER_SIZE:
      raise ValueError(f"UF2 block at {addr:#x} claims oversized payload {size}")
    yield offset, addr, size


def uf2_payload_into(image, data, base, skip_outside=False):
  """Write each UF2 block's payload of `data` into `image` at addr - base.

  skip_outside ignores blocks outside the region: SDK firmware UF2s carry an
  address-wrap marker block at the top of the 16MB flash address space."""
  for offset, addr, size in uf2_blocks(data):
    dest = addr - base
    if dest < 0 or dest + size > len(image):
      if skip_outside:
        continue
      raise ValueError(f"UF2 block at {addr:#x} outside target region")
    image[dest:dest + size] = data[offset + UF2_HEADER_SIZE:
                                   offset + UF2_HEADER_SIZE + size]


def clear_tbyb_in_uf2(data):
  """Return a copy of UF2 `data` with the TBYB flag cleared.

  The block structure — addresses, count, ordering, family ID — is preserved
  exactly; only the payload bytes holding the IMAGE_TYPE flags change. The
  image is flattened first so a picobin item straddling a block boundary is
  still found."""
  blocks = list(uf2_blocks(data))
  if not blocks:
    raise ValueError("UF2 contains no blocks")

  # The address-wrap marker block sits at the top of flash; sizing the flat
  # image to it costs a transient 16MB buffer and keeps the logic simple.
  span = max(addr + size for _, addr, size in blocks) - FLASH_BASE
  image = bytearray(b"\xFF" * span)
  uf2_payload_into(image, data, FLASH_BASE)

  clear_tbyb(image)

  patched = bytearray(data)
  for offset, addr, size in blocks:
    start = addr - FLASH_BASE
    patched[offset + UF2_HEADER_SIZE:offset + UF2_HEADER_SIZE + size] = \
        image[start:start + size]
  return bytes(patched)
