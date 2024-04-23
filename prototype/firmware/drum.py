from .sequencer import Sequencer
from .device_api import Output
from .note_player import NotePlayer
from .tempo import Tempo
from .repeat_effect import RepeatEffect

STEP_COUNT = 8
SEQ_COUNT = 4


class Track:
    def __init__(self, note_player: NotePlayer):
        self.note_player = note_player
        self.note = 0
        self.sequencer = Sequencer(STEP_COUNT)

    def play_step(self, velocity) -> None:
        self.note_player.play(self.note, velocity)


class Drum:
    def __init__(self, output: Output):
        self.tracks = [Track(NotePlayer(ind, output))
                       for ind in range(SEQ_COUNT)]
        self._cur_step_index = 0
        self.tempo = Tempo(self._on_tempo_tick)
        self.playing = True
        self.repeat_effect = RepeatEffect()

    def get_cur_step_index(self):
        return self._cur_step_index

    def update(self) -> None:
        self.tempo.update()

    def _on_tempo_tick(self) -> None:
        if self.playing:
            repeat_step = self.repeat_effect.get_step()
            if repeat_step is not None:
                pass
            else:
                self._cur_step_index = (self._cur_step_index + 1) % STEP_COUNT

            for track in self.tracks:
                step = track.sequencer.steps[self._cur_step_index]
                if step.active:
                    track.play_step(step.velocity)

        for track in self.tracks:
            track.note_player.tick()
