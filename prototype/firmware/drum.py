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
        self.tempo = Tempo(self.__on_tempo_tick)
        self.note_player = note_player

    def update(self):
        self.tempo.update()

    def __on_tempo_tick(self):
        self.drum.__tick_sequencers(self.note_player.play)
        self.note_player.tick()

    def __tick_sequencers(self, play_note_callback):
        def on_seq_note(vel):
            play_note_callback(track.note, vel)

        for track in self.tracks:
            track.sequencer.tick(on_seq_note)
