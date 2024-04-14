from .sequencer import Sequencer
from .device_api import Output
from .note_player import NotePlayer
from .tempo import Tempo

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
        self.tempo = Tempo(self._on_tempo_tick)
        self.playing = True

    def update(self) -> None:
        self.tempo.update()

    def _on_tempo_tick(self) -> None:
        for track in self.tracks:
            track.note_player.tick()
            if self.playing:
                track.sequencer.tick()
