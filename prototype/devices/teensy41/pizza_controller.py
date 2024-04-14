from firmware.device_api import Controller, Output
from firmware.drum import Drum

from .hardware import (
    Teensy41Hardware,
    Display,
    Drumpad,
    SequencerKey,
)

from .reading import IncDecReader, PotReader, bpm_from_pot
from .colors import ColorScheme


# TODO:
# - Move inversion to hardware layer
# - Maybe move jitter prevention to hardware layer

class PizzaController(Controller):
    def __init__(self):
        self.display = Display()

        self.hardware = Teensy41Hardware()

        self.speed_setting = PotReader(self.hardware.speed_pot)
        self.filter_setting = IncDecReader(
            self.hardware.filter_left, self.hardware.filter_right)

    def update(self, drum: Drum, output: Output) -> None:
        self._read_pots(drum, output)
        self._process_keys()

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

    def _read_pots(self, drum: Drum, output: Output) -> None:
        self.speed_setting.read(
            lambda speed: drum.tempo.set_bpm(bpm_from_pot(speed)))

        self.filter_setting.read(
            lambda val: output.adjust_filter(val))

    def _process_keys(self):
        key = self.hardware.get_key_event()
        if key:
            print(f"key: {key}")


def show_track(display, step_color, track, track_index):
    for step_index, step in enumerate(track.sequencer.steps):
        color = None
        if step_index == (track.sequencer.cur_step_index + 7) % 8:
            color = ColorScheme.Cursor
        elif step.active:
            color = step_color

        display.set_color(SequencerKey(step_index, track_index), color)
