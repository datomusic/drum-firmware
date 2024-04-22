from firmware.device_api import Controls, SampleChange, OutputParam
from firmware.controller_api import Controller
from firmware.drum import Drum

import gc

from .colors import ColorScheme
from .hardware import (
    Teensy41Hardware,
    Drumpad,
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


class PizzaController(Controller):
    def __init__(self, hardware=None):
        if hardware is None:
            hardware = Teensy41Hardware()

        self.hardware = hardware
        self.display = hardware.init_display()

        self.speed_setting = PotReader(self.hardware.speed_pot)
        self.volume_setting = PotReader(
            self.hardware.volume_pot)

        self.filter_setting = IncDecReader(
            self.hardware.filter_left, self.hardware.filter_right)

        self.lowpass_setting = PotReader(
            self.hardware.filter_left)

        self.highpass_setting = PotReader(
            self.hardware.filter_right)

        self.lowpass_setting = PotReader(
            self.hardware.filter_left)

        self.highpass_setting = PotReader(
            self.hardware.filter_right)

        self.pitch_settings = [
            PotReader(self.hardware.pitch1, inverted=True),
            PotReader(self.hardware.pitch2, inverted=True),
            PotReader(self.hardware.pitch3, inverted=True),
            PotReader(self.hardware.pitch4, inverted=True)
        ]

        self.drum_triggers = [
            ThresholdTrigger(self.hardware.drum_pad1),
            ThresholdTrigger(self.hardware.drum_pad2),
            ThresholdTrigger(self.hardware.drum_pad3),
            ThresholdTrigger(self.hardware.drum_pad4)
        ]

        self.mute_pads = [
            PotReader(self.hardware.drum_pad1_bottom, inverted=True),
            PotReader(self.hardware.drum_pad2_bottom, inverted=True),
            PotReader(self.hardware.drum_pad3_bottom, inverted=True),
            PotReader(self.hardware.drum_pad4_bottom, inverted=True)
        ]

    def update(self, controls: Controls) -> None:
        gc.collect()
        self._read_pots(controls)
        self._process_keys(controls)

    def show(self, drum: Drum):
        self.display.clear()

        for track_index in range(0, 4):
            self.display.set_color(
                Drumpad(track_index),
                ColorScheme.Tracks[drum.tracks[track_index].note]
            )

            show_track(
                self.display,
                ColorScheme.Tracks[drum.tracks[track_index].note],
                drum.tracks[track_index],
                track_index,
            )

        self.display.show()

    def _read_pots(self, controls: Controls) -> None:
        self.speed_setting.read(
            lambda speed: controls.set_bpm(
                (percentage_from_pot(speed)) * BPM_MAX / 100))

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

        self.highpass_setting.read(
            lambda val: controls.set_output_param(
                OutputParam.HighPass,
                percentage_from_pot(val)))

        self.swing_left.read(
            lambda val: controls.adjust_swing(-0.1))

        self.swing_right.read(
            lambda val: controls.adjust_swing(0.1))

        for track_ind, pitch_setting in enumerate(self.pitch_settings):
            pitch_setting.read(
                lambda pitch: controls.set_track_pitch(
                    track_ind,
                    percentage_from_pot(pitch)))

        for (ind, drum_trigger) in enumerate(self.drum_triggers):
            drum_trigger.read(
                lambda velocity: controls.play_track_sample(
                    ind, percentage_from_pot(velocity)))

        for (ind, muter) in enumerate(self.mute_pads):
            muter.read(
                lambda amount:
                    controls.set_track_mute(
                        ind,
                        100 - percentage_from_pot(amount))
            )

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
                    controls.toggle_playing()


def show_track(display, step_color, track, track_index):
    for step_index, step in enumerate(track.sequencer.steps):
        color = None
        if step_index == (track.sequencer.cur_step_index + 7) % 8:
            color = ColorScheme.Cursor
        elif step.active:
            color = step_color

        display.set_color(SequencerKey(step_index, track_index), color)
