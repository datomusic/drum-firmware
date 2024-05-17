import time
from .drum import Drum
from .device_api import Controls, Output, SampleChange, EffectName, TrackParam
from .controller_api import Controller
from .tempo import Tempo, TempoSource


SAMPLE_COUNT = 32


class AppControls(Controls):
    def __init__(self, drum: Drum, output: Output, tempo: Tempo, on_sample_trigger):
        self.drum = drum
        self.output = output
        self.tempo = tempo
        self.on_sample_trigger = on_sample_trigger

    def set_bpm(self, bpm):
        self.tempo.set_bpm(bpm)

    def toggle_track_step(self, track, step):
        track = self.drum.tracks[track]
        active = track.sequencer.toggle_step(step)

        if active and not self.drum.playing:
            track.note_player.play(track.note)

    def change_sample(self, track_index, change):
        if change == SampleChange.Prev:
            step = -1

        elif change == SampleChange.Next:
            step = 1

        track = self.drum.tracks[track_index]
        if track.note + step < 0:
            track.note = SAMPLE_COUNT - track.note + step
        elif track.note + step >= SAMPLE_COUNT:
            track.note = track.note - SAMPLE_COUNT + step
        else:
            track.note = track.note + step

        if not self.drum.playing:
            track.note_player.play(track.note)

        print(f"Sample change. track: {track_index}, note: {track.note}")

    def set_playing(self, playing):
        self.drum.playing = playing
        if playing:
            self.tempo.reset()

    def is_playing(self):
        return self.drum.playing

    def play_track_sample(self, track_index: int, velocity: float):
        track = self.drum.tracks[track_index]
        track.note_player.play(track.note, velocity)
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
            self.drum.double_time_repeat = False
            if percentage > 96:
                self.drum.repeat_effect.set_repeat_count(1)
                if percentage > 98:
                    self.drum.double_time_repeat = True
            elif percentage > 94:
                self.drum.repeat_effect.set_repeat_count(2)
                self.drum.repeat_effect.set_subdivision(2)
            elif percentage > 20:
                self.drum.repeat_effect.set_repeat_count(3)
                self.drum.repeat_effect.set_subdivision(2)
            else:
                self.drum.repeat_effect.set_repeat_count(0)
                self.drum.repeat_effect.set_subdivision(1)
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


class Application:
    def __init__(self, controllers: list[Controller], output: Output):
        self.controllers = controllers
        self.output = output
        self.tempo = Tempo(
            tempo_tick_callback=self.output.on_tempo_tick,
            on_quarter_beat=self._on_quarter_beat
        )
        self.drum = Drum(output)
        self.drum.playing = False
        self.controls = AppControls(
            self.drum, self.output, self.tempo, self._on_sample_trigger
        )

        setup_tracks(self.drum.tracks)

    def _on_sample_trigger(self, track_index: int):
        for controller in self.controllers:
            controller.on_track_sample_played(track_index)

    def _on_quarter_beat(self, quarter_index) -> None:
        self.drum.tick_beat_repeat(quarter_index)

        if quarter_index % 2 == 0:
            self.drum.advance_step()

    def update(self, delta_ms: int) -> None:
        for controller in self.controllers:
            controller.update(self.controls, delta_ms)

        self.tempo.update()

    def show(self) -> None:
        for controller in self.controllers:
            controller.show(self.drum, self.tempo.get_beat_position())

    def run(self):
        last_ns = time.monotonic_ns()

        while True:
            now = time.monotonic_ns()
            delta_ms = (now - last_ns) // (1000 * 1000)
            last_ns = now
            self.update(delta_ms)
            self.show()


def setup_tracks(tracks):
    tracks[0].note = 4
    tracks[1].note = 12
    tracks[2].note = 20
    tracks[3].note = 28
