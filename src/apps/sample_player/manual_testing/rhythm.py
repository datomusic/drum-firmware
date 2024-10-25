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
        samples = sequence[step % len(sequence)]
        step += 1
        for sample in samples:
            note =  64 + random.randint(-50, 20)
            print(f"Sample: {sample}, Note: {note}")
            output.send_message([0x90 + sample, note, 120])
        time.sleep(1)


if __name__ == "__main__":
    main()


