from .sequencer import Sequencer
from .device_api import Output
from .note_player import NotePlayer
from .repeat_effect import RepeatEffect
from .random_effect import RandomEffect

STEP_COUNT = 8
SAMPLE_COUNT = 32


class Track:
    def __init__(self, note_player: NotePlayer) -> None:
        self.note_player = note_player
        self.note = 0
        self.sequencer = Sequencer(STEP_COUNT)
        self.random_effect = RandomEffect(STEP_COUNT)
        self.repeat_velocity = 0.0

    def trigger_repeat(self, quarter_index) -> bool:
        if self.repeat_is_active():
            is_half = quarter_index % 2 == 0
            if is_half or self.repeat_velocity > 97:
                self.note_player.play(self.note, self.repeat_velocity)
                return True

        return False

    def repeat_is_active(self):
        return self.repeat_velocity > 20

    def play(self, velocity) -> None:
        self.last_velocity = velocity
        self.note_player.play(self.note, velocity)


class Drum:
    def __init__(self, output: Output, track_count: int):
        self.tracks = [Track(NotePlayer(index, output))
                       for index in range(track_count)]
        self._next_step_index = 0
        self.playing = True
        self.repeat_effect = RepeatEffect(lambda: self._next_step_index)
        self.double_time_repeat = False

    def get_indicator_step(self, track_index) -> int:
        effect_step = self._get_effect_step(track_index, -1)
        if effect_step is not None:
            step = effect_step
        else:
            step = (self._get_play_step_index(track_index) - 1)

        return step % STEP_COUNT

    def advance_step(self) -> None:
        if self.playing:
            for track in self.tracks:
                track.random_effect.advance()

            self._play_track_steps()
            self._next_step_index = (self._next_step_index + 1) % STEP_COUNT
            self.repeat_effect.advance()

    def tick_beat_repeat(self, quarter_index):
        if self.playing:
            if self.double_time_repeat:
                self._play_track_steps()

            for (track_index, track) in enumerate(self.tracks):
                track.trigger_repeat(quarter_index)

    def change_sample(self, track_index, step):
        track = self.tracks[track_index]
        if track.note + step < 0:
            track.note = SAMPLE_COUNT - track.note + step
        elif track.note + step >= SAMPLE_COUNT:
            track.note = track.note - SAMPLE_COUNT + step
        else:
            track.note = track.note + step

        if not self.playing:
            track.note_player.play(track.note)

        print(f"Sample change. track: {track_index}, note: {track.note}")

    def toggle_track_step(self, track_index, step_index):
        track = self.tracks[track_index]
        active = track.sequencer.toggle_step(step_index)

        if active and not self.playing:
            track.note_player.play(track.note)

    def set_random_enabled(self, enabled):
        for track in self.tracks:
            track.random_effect.enabled = enabled

    def set_repeat_effect_level(self, percentage: float) -> None:
        self.double_time_repeat = False
        if percentage > 96:
            self.repeat_effect.set_repeat_count(1)
            if percentage > 98:
                self.double_time_repeat = True
        elif percentage > 94:
            self.repeat_effect.set_repeat_count(2)
            self.repeat_effect.set_subdivision(2)
        elif percentage > 20:
            self.repeat_effect.set_repeat_count(3)
            self.repeat_effect.set_subdivision(2)
        else:
            self.repeat_effect.set_repeat_count(0)
            self.repeat_effect.set_subdivision(1)

    def _get_effect_step(self, track_index, offset) -> int | None:
        repeat_step = self.repeat_effect.get_step(offset)
        random_step = self.tracks[track_index].random_effect.get_step()

        if isinstance(repeat_step, int):
            return repeat_step
        elif isinstance(random_step, int):
            return random_step
        else:
            return None

    def _get_play_step_index(self, track_index) -> int:
        step = self._next_step_index
        effect_step = self._get_effect_step(track_index, 0)
        if effect_step is not None:
            step = effect_step

        return step % STEP_COUNT

    def _play_track_steps(self) -> None:
        for index, track in enumerate(self.tracks):
            step = track.sequencer.steps[self._get_play_step_index(index)]
            if step.active:
                track.play(step.velocity)
