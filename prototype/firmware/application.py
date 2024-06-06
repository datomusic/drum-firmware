import time
from .drum import Drum
from .device_api import Controls, Output, EffectName, TrackParam
from .controller_api import Controller
from .tempo import Tempo, TempoSource
import gc


class AppControls(Controls):
    def __init__(self, drum: Drum, output: Output, tempo: Tempo, on_sample_trigger):
        self.drum = drum
        self.output = output
        self.tempo = tempo
        self.on_sample_trigger = on_sample_trigger

    def set_bpm(self, bpm):
        self.tempo.set_bpm(bpm)

    def toggle_track_step(self, track_index, step_index):
        self.drum.toggle_track_step(track_index, step_index)

    def change_sample(self, track_index, change):
        self.drum.change_sample(track_index, change)

    def set_playing(self, playing):
        self.drum.playing = playing
        if playing:
            self.tempo.reset()

    def is_playing(self):
        return self.drum.playing

    def play_track_sample(self, track_index: int, velocity: float):
        self.drum.tracks[track_index].play(velocity)
        self.on_sample_trigger(track_index)

    def set_track_repeat_velocity(self, track_index: int, amount_percent: float):
        self.drum.tracks[track_index].repeat_velocity = amount_percent

    def set_track_param(self, param, track_index: int, amount_percent: float):
        if TrackParam.Pitch == param:
            self.output.set_channel_pitch(track_index, amount_percent)
        elif TrackParam.Mute == param:
            self.output.set_channel_mute(track_index, amount_percent)

    def set_output_param(self, param, percent) -> None:
        self.output.set_param(param, percent)

    def set_effect_level(self, effect_name, percentage):
        if EffectName.Repeat == effect_name:
            self.drum.set_repeat_effect_level(percentage)
        elif EffectName.Random == effect_name:
            self.drum.set_random_enabled(percentage > 50)

    def adjust_swing(self, amount_percent):
        self.tempo.swing.adjust(amount_percent)

    def set_swing(self, amount):
        self.tempo.swing.set_amount(amount)

    def clear_swing(self):
        self.tempo.swing.set_amount(0)

    def handle_midi_clock(self):
        self.tempo.tempo_source = TempoSource.MIDI
        self.tempo.handle_midi_clock()


class Tracker:
    def __init__(self, name):
        self.name = name
        self.reset()

    def start(self):
        self.start_ns = time.monotonic_ns()

    def stop(self):
        self.count += 1
        diff = time.monotonic_ns() - self.start_ns

        if self.average_ns > 0:
            self.average_ns = (self.average_ns + diff) / 2
            self.min = min(self.min, diff)
            self.max = max(self.max, diff)
        else:
            self.average_ns = diff
            self.min = diff
            self.max = diff

        self.total_ns += diff

    def reset(self):
        self.count = 0
        self.start_ns = 0
        self.total_ns = 0
        self.min = 0
        self.max = 0
        self.average_ns = 0

    def get_info(self):
        return f"[{self.name}] count: {self.count}, avg: {self.average_ns / 1_000_000:.2f}ms, min: {self.min / 1_000_000:.2f}ms, max: {self.max / 1_000_000:.2f}ms"


class Timed:
    def __init__(self, name, function):
        self.tracker = Tracker(name)
        self.function = function
        self.name = name

    def __call__(self, *args):
        self.tracker.start()
        self.function(*args)
        self.tracker.stop()


class Application:
    TRACK_COUNT = 4

    def __init__(self, controllers: list[Controller], output: Output):
        self.controllers = controllers
        self.output = output
        self.tempo = Tempo(
            tempo_tick_callback=self.output.on_tempo_tick,
            on_quarter_beat=self._on_quarter_beat
        )
        self.drum = Drum(output, Application.TRACK_COUNT)
        self.drum.playing = False
        self.controls = AppControls(
            self.drum, self.output, self.tempo, self._on_sample_trigger
        )

        setup_tracks(self.drum.tracks)

    def slow_update(self, delta_ms: int) -> None:
        for controller in self.controllers:
            controller.update(self.controls, delta_ms)

    def fast_update(self, delta_ns: int) -> None:
        self.tempo.update(delta_ns)

        delta_ms = delta_ns // 1_000_000
        for controller in self.controllers:
            controller.fast_update(self.controls, delta_ms)

    def show(self, delta_ms) -> None:
        for controller in self.controllers:
            controller.show(self.drum, delta_ms,
                            self.tempo.get_beat_position())

    def run(self):
        for _ in self.run_iterator():
            pass

    def run_iterator(self):
        gc.disable()

        accumulated_info_ns = 0
        accumulated_show_ns = 0
        accumulated_slow_update_ns = 0
        last_ns = time.monotonic_ns()
        frame_counter = 0

        loop_tracker = Tracker("loop")
        gc_collect = Timed("gc_collect", lambda: gc.collect())
        fast_update = Timed("fast_update", self.fast_update)
        slow_update = Timed("slow_update", self.slow_update)
        show = Timed("show", self.show)

        while True:
            loop_tracker.start()

            now = time.monotonic_ns()
            delta_ns = now - last_ns
            accumulated_show_ns += delta_ns
            accumulated_slow_update_ns += delta_ns
            accumulated_info_ns += delta_ns
            last_ns = now
            fast_update(delta_ns)

            if frame_counter % 2 == 0:
                ms = accumulated_slow_update_ns // 1_000_000
                slow_update(ms)
                accumulated_slow_update_ns -= ms * 1_000

            if accumulated_show_ns > 30_000_000:
                show(accumulated_show_ns // 1_000_000)
                accumulated_show_ns = 0
            else:
                gc_collect()

            frame_counter += 1

            loop_tracker.stop()
            if accumulated_info_ns > 1_000_000_000:
                accumulated_info_ns = 0
                print(loop_tracker.get_info())
                loop_tracker.reset()
                for timed in [fast_update, gc_collect, slow_update, show]:
                    print(timed.tracker.get_info())
                    timed.tracker.reset()
                print()

            yield

    def _on_sample_trigger(self, track_index: int):
        for controller in self.controllers:
            controller.on_track_sample_played(track_index)

    def _on_quarter_beat(self, quarter_index) -> None:
        if quarter_index % 2 == 0:
            self.drum.advance_step()

        self.drum.tick_beat_repeat(quarter_index)


def setup_tracks(tracks):
    tracks[0].note = 4
    tracks[1].note = 12
    tracks[2].note = 20
    tracks[3].note = 28
