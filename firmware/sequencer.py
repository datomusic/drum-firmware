from .steps import Steps
from .output_api import Output
from .note_player import NotePlayer
from .repeat_effect import RepeatEffect
from .random_effect import RandomEffect
from .settings import Settings

STEP_COUNT = 8
SAMPLE_COUNT = 32


class Track:
    class PlaySource:
        Manual = 1
        Repeat = 2

    def __init__(self, note_player: NotePlayer) -> None:
        self.note_player = note_player
        self.note = 0
        self.steps = Steps(STEP_COUNT)
        self.random_effect = RandomEffect(STEP_COUNT)
        self.repeat_velocity = 0.0
        self._last_source: int | None = None

    def trigger_repeat(self, quarter_index) -> bool:
        if self.repeat_is_active():
            double_speed = self.repeat_velocity > 97
            is_half = quarter_index % 2 == 0
            last_source_valid = self._check_last_source(
                Track.PlaySource.Repeat)

            if last_source_valid and (is_half or double_speed):
                self.note_player.play(self.note, self.repeat_velocity)
                self._last_source = Track.PlaySource.Repeat
                return True

        self._last_source = None
        return False

    def repeat_is_active(self):
        return self.repeat_velocity > 20

    def play(self, velocity) -> None:
        if self._check_last_source(Track.PlaySource.Manual):
            self._last_source = Track.PlaySource.Manual
            self.last_velocity = velocity
            self.note_player.play(self.note, velocity)

    def _check_last_source(self, source):
        return self._last_source in [None, source]


class Sequencer:
    def __init__(self, output: Output, track_count: int, settings: Settings):
        self.tracks = [Track(NotePlayer(index, output))
                       for index in range(track_count)]
        self.playing = True
        self.settings = settings
        self._next_step_index = 0
        self._repeat_effect = RepeatEffect(lambda: self._next_step_index)
        self._double_time_repeat = False

    def on_quarter_beat(self, quarter_index):
        if quarter_index % 2 == 0:
            self._advance_step()

        self._tick_beat_repeat(quarter_index)

    def get_indicator_step(self, track_index) -> int:
        effect_step = self._get_effect_step(track_index, -1)
        if effect_step is not None:
            step = effect_step
        else:
            step = self._get_play_step_index(track_index) - 1

        return step % STEP_COUNT

    def change_sample(self, track_index, step):
        track = self.tracks[track_index]

        # Get lowest note and maximum note selection range
        track_init_note = int(self.settings.get(
            f"track.{track_index}.init_note"))
        range = int(self.settings.get(f"track.{track_index}.range"))

        # Wrap the values around when they hit the min or max allowed note
        track.note = (
            track_init_note + (((track.note - track_init_note) + step) % range)
        ) % SAMPLE_COUNT

        # track.note = (track.note + step) % SAMPLE_COUNT

        if not self.playing:
            track.note_player.play(track.note)

        print(f"Sample change. track: {track_index}, note: {track.note}")

    def toggle_track_step(self, track_index, step_index):
        track = self.tracks[track_index]
        active = track.steps.toggle_step(step_index)
        if active and not self.playing:
            track.note_player.play(track.note)

    def set_random_enabled(self, enabled):
        for track in self.tracks:
            track.random_effect.enabled = enabled

    def set_repeat_effect_level(self, percentage: float) -> None:
        self._double_time_repeat = False
        if percentage > 97:
            self._repeat_effect.set_repeat_count(1)
        # elif percentage > 90:
        #     self._repeat_effect.set_repeat_count(2)
        #     self._repeat_effect.set_subdivision(2)
        elif percentage > 20:
            self._repeat_effect.set_repeat_count(3)
            self._repeat_effect.set_subdivision(2)
        else:
            self._repeat_effect.set_repeat_count(0)
            self._repeat_effect.set_subdivision(1)

    def _advance_step(self) -> None:
        if self.playing:
            for track in self.tracks:
                track.random_effect.advance()

            self._play_track_steps()
            self._next_step_index = (self._next_step_index + 1) % STEP_COUNT
            self._repeat_effect.advance()

    def _tick_beat_repeat(self, quarter_index):
        if self.playing:
            if self._double_time_repeat:
                self._play_track_steps()

            for track_index, track in enumerate(self.tracks):
                track.trigger_repeat(quarter_index)

    def _get_effect_step(self, track_index, offset) -> int | None:
        repeat_step = self._repeat_effect.get_step(offset)
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
            step = track.steps.entries[self._get_play_step_index(index)]
            if step.active:
                track.play(step.velocity)
