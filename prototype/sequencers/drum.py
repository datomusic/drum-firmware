from sequencer import Sequencer

SEQ_COUNT = 4


class Track:
    def __init__(self):
        self.sequencer = Sequencer()
        self.note = 0


class Drum:
    def __init__(self):
        self.tracks = [Track() for _ in range(SEQ_COUNT)]

    def tick(self, play_note_callback):
        def on_seq_note(vel):
            play_note_callback(track_ind, track.note, vel)

        for track_ind, track in enumerate(self.tracks):
            track.sequencer.tick(on_seq_note)
