import time
from drum import Drum

tick = 0


def output_midi_note(track, note, velocity):
    print(f"[{tick}] Seq: {track}, Note: {note}, vel: {velocity}")


drum = Drum()
drum.tracks[2].note = 42
drum.tracks[2].sequencer.set_step(4, 66)

while True:
    print(time.monotonic_ns())
    time.sleep(0.1)
    drum.tick(output_midi_note)
    tick += 1
