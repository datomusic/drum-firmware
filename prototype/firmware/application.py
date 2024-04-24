from .drum import Drum
from .device_api import Controls, Output, SampleChange, EffectName
from .controller_api import Controller
from .tempo import Tempo


SAMPLE_COUNT = 32


class AppControls(Controls):
    def __init__(self, drum: Drum, output: Output, tempo: Tempo):
        self.drum = drum
        self.output = output
        self.tempo = tempo

    def set_bpm(self, bpm):
        self.tempo.set_bpm(bpm)

    def toggle_track_step(self, track, step):
        self.drum.tracks[track].sequencer.toggle_step(step)

    def set_track_pitch(self, track_index, pitch):
        self.output.set_channel_pitch(track_index, pitch)

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
        print(f"Sample change. track: {track_index}, note: {track.note}")

    def toggle_playing(self):
        self.drum.playing = not self.drum.playing

    def play_track_sample(self, track_index: int, velocity: float):
        track = self.drum.tracks[track_index]
        track.note_player.play(track.note, velocity)

    def set_track_mute(self, track_index: int, amount_percent: float):
        track = self.drum.tracks[track_index]
        track.note_player.mute_level = amount_percent

    def set_output_param(self, param, percent) -> None:
        self.output.set_param(param, percent)

    def set_effect_level(self, effect_name, percentage):
        if EffectName.Repeat == effect_name:
            print(percentage)
            if percentage > 99:
                self.drum.repeat_effect.set_repeat_count(1)
                self.drum.repeat_effect.set_repeat_divider(2)
            elif percentage > 94:
                self.drum.repeat_effect.set_repeat_count(2)
                self.drum.repeat_effect.set_repeat_divider(2)
            elif percentage > 30:
                self.drum.repeat_effect.set_repeat_count(3)
                self.drum.repeat_effect.set_repeat_divider(2)
            else:
                self.drum.repeat_effect.set_repeat_count(0)
                self.drum.repeat_effect.set_repeat_divider(1)
        elif EffectName.Random == effect_name:
            self.drum.random_effect.enabled = percentage > 50


    def adjust_swing(self, amount_percent):
        self.tempo.internal_tempo.adjust_swing(amount_percent)

    def reset_swing(self):
        self.tempo.internal_tempo.reset_swing()

class Application:
    def __init__(self, controllers: list[Controller], output: Output):
        self.controllers = controllers
        self.output = output
        self.tempo = Tempo(on_tick=self._on_tick,
                           on_half_beat=self._on_half_beat)
        self.drum = Drum(output)
        self.drum.playing = False
        self.controls = AppControls(self.drum, self.output, self.tempo)

        setup_tracks(self.drum.tracks)

    def _on_tick(self, source) -> None:
        self.output.on_tempo_tick(source)
        self.drum.tick()

    def _on_half_beat(self) -> None:
        self.drum.advance_step()

    def update(self) -> None:
        for controller in self.controllers:
            controller.update(self.controls)

        self.tempo.update()

    def show(self) -> None:
        for controller in self.controllers:
            controller.show(self.drum)

    def run(self):
        while True:
            self.update()
            self.show()


def setup_tracks(tracks):
    tracks[0].note = 10
    tracks[1].note = 0
    tracks[2].note = 18
    tracks[3].note = 25