from sequencer import Sequencer

SEQ_COUNT = 4


class Drum:
    def __init__(self):
        self.sequencers = [Sequencer() for _ in range(SEQ_COUNT)]

    def tick(self, play_note):
        seq_ind = 0

        def on_seq_note(note, vel):
            play_note(seq_ind, note, vel)

        for ind, seq in enumerate(self.sequencers):
            seq_ind = ind
            seq.tick(on_seq_note)
