from .sequencer import Sequencer
from .note_player import NotePlayer
from .tempo import Tempo

SEQ_COUNT = 4


class Track:
    def __init__(self):
        self.sequencer = Sequencer()
        self.note = 0


class Drum:
    def __init__(self, note_player: NotePlayer):
        self.tracks = [Track() for _ in range(SEQ_COUNT)]
        self.tempo = Tempo(self._on_tempo_tick)
        self.note_player = note_player
        self.playing = True

    def update(self) -> None:
        self.tempo.update()

    def _on_tempo_tick(self) -> None:
        if self.playing:
            self._tick_sequencers(self.note_player.play)

        self.note_player.tick()

    def _tick_sequencers(self, play_note_callback) -> None:
        def on_seq_note(vel):
            play_note_callback(track.note, vel)

        for track in self.tracks:
            track.sequencer.tick(on_seq_note)
