import time
from drum import Drum

tick = 0


def play_note(seq_id, note, velocity):
    print(f"[{tick}] Seq: {seq_id}, Note: {note}, vel: {velocity}")


drum = Drum()
drum.sequencers[0].step_on(4, 66)

while True:
    time.sleep(0.1)
    drum.tick(play_note)
    tick += 1
