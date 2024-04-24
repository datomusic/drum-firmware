from .sequencer import Sequencer
from .device_api import Output
from .note_player import NotePlayer

SEQ_COUNT = 4


class Track:
    def __init__(self, note_player: NotePlayer):
        self.note_player = note_player
        self.note = 0

        def play_step(velocity) -> None:
            self.note_player.play(self.note, velocity)

        self.sequencer = Sequencer(play_step)


class Drum:
    def __init__(self, output: Output):
        self.tracks = [Track(NotePlayer(ind, output))
                       for ind in range(SEQ_COUNT)]
        self.playing = True

    def advance_step(self) -> None:
        for track in self.tracks:
            track.note_player.tick()
            if self.playing:
                track.sequencer.tick()
