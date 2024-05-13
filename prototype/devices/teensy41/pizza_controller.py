from firmware.device_api import Controls, SampleChange, OutputParam, TrackParam, EffectName
from firmware.controller_api import Controller
from firmware.drum import Drum
from .pizza_view import PizzaView

import gc

from .hardware import (
    Teensy41Hardware,
    SequencerKey,
    SampleSelectKey,
    Direction,
    ControlKey,
    ControlName
)

from .reading import (
    PotReader,
    IncDecReader,
    DigitalTrigger,
    ThresholdTrigger,
    percentage_from_pot)

BPM_MAX = 300

# TODO:
# - Move inversion to hardware layer
# - Maybe move jitter prevention to hardware layer


class DrumPad:
    def __init__(self, track_index, trigger_port, mute_port):
        self.track_index = track_index
        self.trigger = ThresholdTrigger(trigger_port)
        self.mute = PotReader(mute_port, inverted=True)
        self.muted_when_triggered = False

    def update(self, controls):
        was_triggered = self.trigger.triggered
        triggered, value = self.trigger.read()
        trigger_changed = was_triggered != self.trigger.triggered

        if not self.trigger.triggered or self.muted_when_triggered:
            self.mute.read(
                lambda amount:
                    controls.set_track_param(
                        TrackParam.Mute,
                        self.track_index,
                        100 - percentage_from_pot(amount))
            )

        velocity = percentage_from_pot(value)
        muted = self.mute.last_val > 1000

        if trigger_changed:
            if self.trigger.triggered:
                self.muted_when_triggered = muted
            else:
                controls.set_track_repeat_velocity(self.track_index, 0)
                self.muted_when_triggered = False

        elif velocity > 1 and not (muted or self.muted_when_triggered):
            controls.set_track_repeat_velocity(self.track_index, velocity)

        if triggered and not muted:
            controls.play_track_sample(self.track_index, velocity)


class PizzaController(Controller):
    def __init__(self, hardware=None) -> None:
        if hardware is None:
            hardware = Teensy41Hardware()

        self.hardware = hardware
        self.display = hardware.init_display()
        self.view = PizzaView()

        self.speed_setting = PotReader(self.hardware.speed_pot)
        self.volume_setting = PotReader(self.hardware.volume_pot)

        self.filter_setting = IncDecReader(
            self.hardware.filter_left, self.hardware.filter_right)

        self.lowpass_setting = PotReader(self.hardware.filter_left)
        self.highpass_setting = PotReader(self.hardware.filter_right)
        self.beat_repeat_setting = PotReader(self.hardware.repeat_button)
        self.random_setting = PotReader(self.hardware.random_button)
        self.highpass_setting = PotReader(
            self.hardware.filter_right)

        self.swing_left = DigitalTrigger(self.hardware.swing_left)
        self.swing_right = DigitalTrigger(self.hardware.swing_right)

        self.pitch_settings = [
            PotReader(self.hardware.pitch1, inverted=True),
            PotReader(self.hardware.pitch2, inverted=True),
            PotReader(self.hardware.pitch3, inverted=True),
            PotReader(self.hardware.pitch4, inverted=True)
        ]

        self.drum_pads = [
            DrumPad(0, self.hardware.drum_pad1,
                    self.hardware.drum_pad1_bottom),
            DrumPad(1, self.hardware.drum_pad2,
                    self.hardware.drum_pad2_bottom),
            DrumPad(2, self.hardware.drum_pad3,
                    self.hardware.drum_pad3_bottom),
            DrumPad(3, self.hardware.drum_pad4, self.hardware.drum_pad4_bottom)
        ]

    def update(self, controls: Controls, delta_ms: int) -> None:
        gc.collect()
        self._read_pots(controls)
        self._process_keys(controls)
        self.view.update(delta_ms)

    def show(self, drum: Drum, beat_position: float) -> None:
        self.display.clear()
        self.view.show(self.display, drum, beat_position)
        self.display.show()

    def on_track_sample_played(self, track_index: int):
        self.view.trigger_track(track_index)

    def _read_pots(self, controls: Controls) -> None:
        self.speed_setting.read(
            lambda speed: (
                controls.set_bpm((percentage_from_pot(speed)) * BPM_MAX / 100),
                controls.set_output_param(OutputParam.Tempo, percentage_from_pot(speed))))

        self.volume_setting.read(
            lambda vol: controls.set_output_param(
                OutputParam.Volume,
                percentage_from_pot(vol)))

        self.filter_setting.read(
            lambda val: controls.set_output_param(
                OutputParam.AdjustFilter,
                percentage_from_pot(val) / 50))

        self.lowpass_setting.read(
            lambda val: controls.set_output_param(
                OutputParam.LowPass,
                percentage_from_pot(val)))

        self.highpass_setting.read(
            lambda val: controls.set_output_param(
                OutputParam.HighPass,
                percentage_from_pot(val)))

        self.swing_left.read(
            lambda val: controls.adjust_swing(-10))

        self.swing_right.read(
            lambda val: controls.adjust_swing(10))

        if self.swing_left.triggered and self.swing_right.triggered:
            controls.clear_swing()

        self.highpass_setting.read(
            lambda val: controls.set_output_param(
                OutputParam.HighPass,
                percentage_from_pot(val)))

        self.beat_repeat_setting.read(
            lambda val: controls.set_effect_level(
                EffectName.Repeat,
                percentage_from_pot(val)))

        self.random_setting.read(
            lambda val: controls.set_effect_level(
                EffectName.Random,
                percentage_from_pot(val)))

        for track_ind, pitch_setting in enumerate(self.pitch_settings):
            pitch_setting.read(
                lambda pitch: controls.set_track_param(
                    TrackParam.Pitch,
                    track_ind,
                    percentage_from_pot(pitch)))

        for (track_index, pad) in enumerate(self.drum_pads):
            pad.update(controls)

    def _process_keys(self, controls: Controls) -> None:
        event = self.hardware.get_key_event()
        pressed = event and event.pressed
        if pressed:
            key = event.key
            if isinstance(key, SequencerKey):
                controls.toggle_track_step(key.track, key.step)

            elif isinstance(key, SampleSelectKey):
                if key.direction == Direction.Down:
                    change = SampleChange.Prev
                elif key.direction == Direction.Up:
                    change = SampleChange.Next

                controls.change_sample(key.track, change)

            elif isinstance(key, ControlKey):
                if key.name == ControlName.Start:
                    controls.set_playing(not controls.is_playing())
