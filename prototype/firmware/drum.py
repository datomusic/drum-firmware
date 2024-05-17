from .sequencer import Sequencer
from .device_api import Output
from .note_player import NotePlayer
from .repeat_effect import RepeatEffect
from .random_effect import RandomEffect

STEP_COUNT = 8


class Track:
    Count = 4

    def __init__(self, note_player: NotePlayer):
        self.note_player = note_player
        self.note = 0
        self.sequencer = Sequencer(STEP_COUNT)
        self.random_effect = RandomEffect(STEP_COUNT)
        self.repeat_velocity = 0.0

    def tick(self):
        self.note_player.tick()
        self.random_effect.tick()

    def trigger_repeat(self, quarter_index):
        if self.repeat_is_active():
            is_half = quarter_index % 2 == 0
            if is_half or self.repeat_velocity > 95:
                self.note_player.play(self.note, self.repeat_velocity)
                return True
        else:
            return False

    def repeat_is_active(self):
        return self.repeat_velocity > 20

    def play_step(self, velocity) -> None:
        self.last_velocity = velocity
        self.note_player.play(self.note, velocity)


class Drum:
    def __init__(self, output: Output):
        self.tracks = [Track(NotePlayer(index, output))
                       for index in range(Track.Count)]
        self._next_step_index = 0
        self.playing = True
        self.repeat_effect = RepeatEffect(lambda: self._next_step_index)

    def _get_effect_step(self, track_index) -> int | None:
        repeat_step = self.repeat_effect.get_step()
        random_step = self.tracks[track_index].random_effect.get_step()

        if isinstance(repeat_step, int):
            return repeat_step
        elif isinstance(random_step, int):
            return random_step
        else:
            return None

    def _get_play_step_index(self, track_index) -> int:
        step = self._next_step_index
        effect_step = self._get_effect_step(track_index)
        if effect_step is not None:
            step = effect_step

        return step % STEP_COUNT

    def get_indicator_step(self, track_index) -> int:
        effect_step = self._get_effect_step(track_index)
        if effect_step is not None:
            step = effect_step
        else:
            step = (self._get_play_step_index(track_index) - 1)

        return step % STEP_COUNT

    def advance_step(self) -> None:
        if self.playing:
            self._play_track_steps()
            self._next_step_index = (self._next_step_index + 1) % STEP_COUNT
            self.repeat_effect.advance()
            for track in self.tracks:
                track.tick()

    def tick_beat_repeat(self, quarter_index):
        for (track_index, track) in enumerate(self.tracks):
            if self.playing:
                track.trigger_repeat(quarter_index)

    def _play_track_steps(self) -> None:
        for index, track in enumerate(self.tracks):
            step = track.sequencer.steps[self._get_play_step_index(index)]
            if step.active:
                track.play_step(step.velocity)

    def set_random_enabled(self, enabled):
        for track in self.tracks:
            track.random_effect.enabled = enabled
