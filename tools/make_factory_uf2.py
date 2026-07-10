#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = [
#   # Pinned: the vendored littlefs must write the same disk format as the
#   # littlefs in pico-vfs (currently disk version 2.1). A mismatched image
#   # makes the firmware reformat the data partition, discarding the samples.
#   # After changing this pin or updating pico-vfs, verify a composed image
#   # mounts on hardware before shipping.
#   "littlefs-python==0.18.0",
# ]
# ///
"""Compose a factory-flash UF2 from its parts, without a golden device.

Run with `uv run tools/make_factory_uf2.py ...` to get the pinned
littlefs-python automatically, or install it into your environment yourself.

Assembles into one sparse, absolute-family UF2:
  - the partition table (created via `picotool partition create`)
  - the firmware image, placed in both partition A and B
  - a littlefs filesystem built on the host from the factory sample WAVs,
    placed in the data partition

Sample WAVs must be 16-bit mono; the slot number is taken from the leading
digits of the filename (e.g. `30_kick_disco.wav` -> `/30.pcm`), matching the
naming the SDS receive path uses on the device.

The littlefs geometry (4096-byte blocks, 256-byte program size) and disk
version must match the littlefs vendored in pico-vfs; a mismatch makes the
firmware treat the partition as corrupt and reformat it, silently discarding
the samples. Verify on hardware after changing littlefs-python or pico-vfs.

Requires: picotool on PATH.

Usage:
  uv run tools/make_factory_uf2.py --firmware drum/build/drum-1.0.0-rc.2.uf2 \
      --samples samples/factory_kit -o drum-factory.uf2
"""

import argparse
import json
import pathlib
import re
import struct
import subprocess
import sys
import tempfile
import wave

FLASH_BASE = 0x10000000
PARTITION_TABLE_REGION = 0x2000  # flash reserved for the partition table
BLOCK_SIZE = 4096  # littlefs block == flash sector
PROG_SIZE = 256  # flash page
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID = 0x00002000
FAMILY_ABSOLUTE = 0xE48BFF57
UF2_PAYLOAD = 256


def parse_size(text):
  match = re.fullmatch(r"(\d+)([KM]?)", text)
  if not match:
    raise ValueError(f"unsupported partition size: {text}")
  return int(match.group(1)) * {"": 1, "K": 1024, "M": 1024 * 1024}[match.group(2)]


def partition_offsets(table_json_path):
  """Sequential 4K-aligned allocation after the partition-table region,
  mirroring picotool's layout. Returns [(offset, size), ...] by id order."""
  with open(table_json_path) as f:
    table = json.load(f)
  offsets = []
  cursor = PARTITION_TABLE_REGION
  for part in table["partitions"]:
    size = parse_size(part["size"])
    offsets.append((cursor, size))
    cursor += size
  return offsets


def uf2_payload_into(image, data, base, skip_outside=False):
  """Write each UF2 block's payload of `data` into `image` at addr - base.

  skip_outside ignores blocks outside the region: SDK firmware UF2s carry an
  address-wrap marker block at the top of the 16MB flash address space."""
  for offset in range(0, len(data), 512):
    block = data[offset:offset + 512]
    magic0, magic1, _flags, addr, size = struct.unpack_from("<5I", block, 0)
    if (magic0, magic1) != (UF2_MAGIC_START0, UF2_MAGIC_START1):
      raise ValueError(f"bad UF2 magic at offset {offset:#x}")
    dest = addr - base
    if dest < 0 or dest + size > len(image):
      if skip_outside:
        continue
      raise ValueError(f"UF2 block at {addr:#x} outside target region")
    image[dest:dest + size] = block[32:32 + size]


def create_partition_table(table_json_path):
  with tempfile.NamedTemporaryFile(suffix=".uf2") as tmp:
    subprocess.run(
        ["picotool", "partition", "create", str(table_json_path), tmp.name],
        check=True, capture_output=True, text=True)
    return pathlib.Path(tmp.name).read_bytes()


def wav_to_pcm(path):
  with wave.open(str(path)) as w:
    if w.getnchannels() != 1 or w.getsampwidth() != 2:
      raise ValueError(f"{path.name}: expected 16-bit mono WAV, got "
                       f"{w.getnchannels()} ch / {w.getsampwidth() * 8}-bit")
    return w.readframes(w.getnframes())


PICOBIN_BLOCK_MARKER_START = 0xFFFFDED3
PICOBIN_ITEM_TYPE_IMAGE_TYPE = 0x42
PICOBIN_IMAGE_TYPE_EXE_TBYB_BITS = 0x8000


def clear_tbyb(firmware):
  """Clear the try-before-you-buy flag in the picobin IMAGE_TYPE item.

  The build sets TBYB so A/B updates boot as flash-update trials, but the
  bootrom refuses to boot an unbought TBYB image on a normal power-on. The
  factory image must ship pre-bought — the same state rom_explicit_buy
  leaves in flash after a successful update."""
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


# Disk format written by the littlefs vendored in pico-vfs
# (musin/ports/pico/libraries/pico-vfs/vendor/littlefs, LFS_DISK_VERSION).
PICO_VFS_LFS_DISK_VERSION = (2, 1)


def build_littlefs_image(samples_dir, partition_size):
  import littlefs
  from littlefs import LittleFS

  if littlefs.__LFS_DISK_VERSION__ != PICO_VFS_LFS_DISK_VERSION:
    raise RuntimeError(
        f"littlefs-python {littlefs.__version__} writes disk format "
        f"{littlefs.__LFS_DISK_VERSION__}, but pico-vfs expects "
        f"{PICO_VFS_LFS_DISK_VERSION}; the firmware would reformat the "
        "data partition on first mount")

  fs = LittleFS(block_size=BLOCK_SIZE,
                block_count=partition_size // BLOCK_SIZE,
                prog_size=PROG_SIZE, read_size=PROG_SIZE)
  wavs = sorted(pathlib.Path(samples_dir).glob("*.wav"))
  if not wavs:
    raise ValueError(f"no .wav files in {samples_dir}")
  for wav_path in wavs:
    match = re.match(r"(\d+)", wav_path.stem)
    if not match:
      raise ValueError(f"{wav_path.name}: filename must start with slot number")
    slot = int(match.group(1))
    with fs.open(f"/{slot:02d}.pcm", "wb") as f:
      f.write(wav_to_pcm(wav_path))
    print(f"  /{slot:02d}.pcm <- {wav_path.name}")
  return bytes(fs.context.buffer)


def emit_uf2(image, path):
  """Emit `image` (flash-relative) as a sparse absolute-family UF2,
  skipping erased (all-0xFF) 256-byte pages."""
  pages = [offset for offset in range(0, len(image), UF2_PAYLOAD)
           if image[offset:offset + UF2_PAYLOAD].count(0xFF) != UF2_PAYLOAD]
  out = bytearray()
  for index, offset in enumerate(pages):
    block = bytearray(512)
    struct.pack_into("<8I", block, 0, UF2_MAGIC_START0, UF2_MAGIC_START1,
                     UF2_FLAG_FAMILY_ID, FLASH_BASE + offset, UF2_PAYLOAD,
                     index, len(pages), FAMILY_ABSOLUTE)
    block[32:32 + UF2_PAYLOAD] = image[offset:offset + UF2_PAYLOAD]
    struct.pack_into("<I", block, 508, UF2_MAGIC_END)
    out += block
  pathlib.Path(path).write_bytes(out)
  return len(pages)


def main():
  repo_root = pathlib.Path(__file__).resolve().parent.parent
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("--firmware", required=True,
                      help="firmware UF2 from drum/build.sh (linked at flash base)")
  parser.add_argument("--samples", default=str(repo_root / "samples/factory_kit"),
                      help="directory of NN_name.wav factory samples")
  parser.add_argument("--partition-table",
                      default=str(repo_root / "drum/partition_table.json"))
  parser.add_argument("-o", "--output", default="drum-factory.uf2")
  args = parser.parse_args()

  offsets = partition_offsets(args.partition_table)
  (fw_a_offset, fw_a_size), (fw_b_offset, _), (data_offset, data_size) = offsets
  flash_size = data_offset + data_size
  image = bytearray(b"\xFF" * flash_size)

  print("Partition table...")
  uf2_payload_into(image, create_partition_table(args.partition_table), FLASH_BASE)

  print(f"Firmware -> A @ {fw_a_offset:#x} and B @ {fw_b_offset:#x}...")
  firmware = bytearray(b"\xFF" * fw_a_size)
  uf2_payload_into(firmware, pathlib.Path(args.firmware).read_bytes(), FLASH_BASE,
                   skip_outside=True)
  clear_tbyb(firmware)
  used = max((i for i, b in enumerate(firmware) if b != 0xFF), default=-1) + 1
  image[fw_a_offset:fw_a_offset + used] = firmware[:used]
  image[fw_b_offset:fw_b_offset + used] = firmware[:used]

  print(f"Factory samples -> littlefs @ {data_offset:#x} ({data_size // 1024}K)...")
  image[data_offset:data_offset + data_size] = \
      build_littlefs_image(args.samples, data_size)

  blocks = emit_uf2(image, args.output)
  print(f"wrote {args.output}: {blocks} blocks, {blocks * 512 / 1e6:.1f} MB")


if __name__ == "__main__":
  sys.exit(main())
