from .sequencer import Sequencer
from .device_api import Output
from .note_player import NotePlayer
from .repeat_effect import RepeatEffect
from .random_effect import RandomEffect

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
        self.tracks = [Track(NotePlayer(ind, output)) for ind in range(SEQ_COUNT)]
        self._cur_step_index = 0
        self.playing = True
        self.repeat_effect = RepeatEffect(lambda: self._cur_step_index)
        self.random_effect = RandomEffect(STEP_COUNT)

    def get_cur_step_index(self):
        step = self._cur_step_index
        repeat_step = self.repeat_effect.get_step()
        random_step = self.random_effect.get_step()

        if isinstance(repeat_step, int):
            step = repeat_step
        elif isinstance(random_step, int):
            step = random_step

        return step % STEP_COUNT

    def advance_step(self) -> None:
        if self.playing:
            self.repeat_effect.tick()
            self.random_effect.tick()
            self._cur_step_index = (self._cur_step_index + 1) % STEP_COUNT

            for track in self.tracks:
                step = track.sequencer.steps[self.get_cur_step_index()]
                if step.active:
                    track.play_step(step.velocity)

        for track in self.tracks:
            track.note_player.tick()
