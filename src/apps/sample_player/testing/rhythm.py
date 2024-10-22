import rtmidi
from rtmidi.midiutil import open_midioutput
import time
import random

def find_duo_midi_port():
    for (index, name) in enumerate(rtmidi.MidiOut().get_ports()):
        if "duo" in name.lower():
            print(f"Found {name} connected to MIDI")
            return index
    return None


sequence = [(0,), (1,), (2,), (3,)]

def main():
    port = find_duo_midi_port()
    output, _portname = open_midioutput(port, use_virtual=False)

    step = 0

    while True:
        time.sleep(0.3)
        samples = sequence[step % len(sequence)]
        step += 1
        for sample in samples:
            note =  64 + random.randint(-60, 60)
            output.send_message([0x90 + sample, note, 120])


if __name__ == "__main__":
    main()


