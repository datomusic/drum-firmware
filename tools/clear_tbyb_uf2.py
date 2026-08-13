#!/usr/bin/env python3
"""Pre-buy a firmware UF2 so it boots after a direct flash.

`drum/build.sh` produces images with the picobin try-before-you-buy bit set,
because the A/B update path boots them as flash-update trials and
`drum/firmware_update_buyer.cpp` commits them with rom_explicit_buy once the
firmware has run healthily. A plain `picotool load` is not an update boot, so
the image is never bought and the bootrom refuses it on the next power-on —
the device comes up dead: no USB, no LEDs.

Clearing the bit here leaves the image in exactly the state a successful
update leaves in flash, so it survives a power cycle.

Note the trade-off: a pre-bought image cannot exercise the trial-boot and
rollback path. Test A/B updates with an unpatched build.

Usage:
  python3 tools/clear_tbyb_uf2.py in.uf2 -o out.uf2
"""

import argparse
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from picobin import clear_tbyb_in_uf2  # noqa: E402


def main():
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("firmware", help="firmware UF2 from a build script")
  parser.add_argument("-o", "--output", required=True)
  args = parser.parse_args()

  source = pathlib.Path(args.firmware)
  try:
    patched = clear_tbyb_in_uf2(source.read_bytes())
  except ValueError as error:
    print(f"error: {source}: {error}", file=sys.stderr)
    return 1

  pathlib.Path(args.output).write_bytes(patched)
  print(f"Pre-bought (TBYB cleared): {args.output}")
  return 0


if __name__ == "__main__":
  sys.exit(main())
