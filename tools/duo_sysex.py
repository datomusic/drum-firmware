#!/usr/bin/env python3
"""Exercise the DUO's SysEx commands over USB MIDI.

The DUO speaks the legacy dialect inherited from the Brains 2 firmware:
manufacturer ID 0x7D (the single-byte non-commercial ID) and device ID 0x64.
This is NOT the DRUM's dialect, which uses Dato's real three-byte
manufacturer ID 00 22 01 with device ID 0x65 — so tools/drumtool cannot talk
to the DUO, and vice versa. See docs/duo-rp2350-port.md §12.4.

Requires python-rtmidi.

Usage:
  duo_sysex.py                 # run every query and print the decoded reply
  duo_sysex.py version
  duo_sysex.py identity
  duo_sysex.py serial
  duo_sysex.py transpose       # reset transpose to 0 (no reply)
  duo_sysex.py bootloader      # reboot into BOOTSEL (no reply)
  duo_sysex.py --port DUO      # pick a port by substring
"""

import argparse
import sys
import time

try:
    import rtmidi
except ImportError:
    sys.exit("python-rtmidi is required: pip install python-rtmidi")

DATO_ID = 0x7D
DUO_ID = 0x64
UNIVERSAL_NONREALTIME_ID = 0x7E

QUERIES = {
    "version": [0xF0, DATO_ID, DUO_ID, 0x01, 0xF7],
    "serial": [0xF0, DATO_ID, DUO_ID, 0x02, 0xF7],
    "identity": [0xF0, UNIVERSAL_NONREALTIME_ID, DUO_ID, 0x06, 0x01, 0xF7],
}
COMMANDS = {
    "bootloader": [0xF0, DATO_ID, DUO_ID, 0x0B, 0xF7],
    "transpose": [0xF0, DATO_ID, DUO_ID, 0x0C, 0xF7],
}


def hexs(data):
    return " ".join(f"{b:02x}" for b in data)


def decode(reply):
    """Return a human-readable reading of a known reply, or None."""
    if len(reply) == 7 and reply[1] == DATO_ID and reply[2] == DUO_ID:
        return f"firmware {reply[3]}.{reply[4]}.{reply[5]}"

    if len(reply) == 14 and reply[1] == UNIVERSAL_NONREALTIME_ID:
        return (f"identity: manufacturer {reply[5]:#04x}, "
                f"firmware {reply[10]}.{reply[11]}.{reply[12]}")

    if len(reply) == 24 and reply[1] == DATO_ID and reply[2] == DUO_ID:
        # 4 groups of 5 right-aligned 7-bit values. The RP2350's board id is
        # 64 bits where the i.MX's was 128, so the top two groups are zero.
        words = []
        for group in range(4):
            value = 0
            for digit in range(5):
                value = (value << 7) | reply[3 + group * 5 + digit]
            words.append(value & 0xFFFFFFFF)
        return "serial " + "".join(f"{w:08x}" for w in words)

    return None


def open_ports(substring):
    midi_out, midi_in = rtmidi.MidiOut(), rtmidi.MidiIn()
    midi_in.ignore_types(sysex=False)

    ports = midi_out.get_ports()
    if not ports:
        sys.exit("no MIDI output ports found")

    index = 0
    if substring:
        matches = [i for i, p in enumerate(ports) if substring.lower() in p.lower()]
        if not matches:
            sys.exit(f"no port matching {substring!r}; available: {ports}")
        index = matches[0]

    midi_out.open_port(index)
    midi_in.open_port(index)
    time.sleep(0.3)  # CoreMIDI needs a moment before the first send
    return midi_out, midi_in, ports[index]


def request(midi_out, midi_in, message, timeout=1.0):
    while midi_in.get_message():
        pass
    midi_out.send_message(message)

    deadline = time.time() + timeout
    while time.time() < deadline:
        received = midi_in.get_message()
        if received:
            return received[0]
        time.sleep(0.005)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", nargs="?", default="all",
                        choices=["all", *QUERIES, *COMMANDS])
    parser.add_argument("--port", help="substring of the MIDI port name")
    args = parser.parse_args()

    midi_out, midi_in, port_name = open_ports(args.port)
    print(f"port: {port_name}\n")

    if args.command in COMMANDS:
        midi_out.send_message(COMMANDS[args.command])
        print(f"{args.command:10} sent {hexs(COMMANDS[args.command])} (no reply expected)")
        time.sleep(0.5)
        return 0

    names = list(QUERIES) if args.command == "all" else [args.command]
    failures = 0
    for name in names:
        reply = request(midi_out, midi_in, QUERIES[name])
        if reply is None:
            print(f"{name:10} NO REPLY")
            failures += 1
            continue
        reading = decode(reply)
        print(f"{name:10} {hexs(reply)}")
        if reading:
            print(f"{'':10} -> {reading}")

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
