from firmware.device_api import Controller
from firmware.drum import Drum

from .hardware import (
    Controls,
    Display,
    Drumpad,
    SequencerKey,
    PotName,
)

from .colors import ColorScheme

BPM_MAX = 500
POT_MIN = 0
POT_MAX = 65536


class PizzaController(Controller):
    def __init__(self):
        self.controls = Controls()
        self.display = Display()
        self.speed_pot = PotReader(
            lambda: self.controls.read_pot(PotName.Speed))

    def update(self, drum: Drum) -> None:
        self.__read_pots(drum)

    def show(self, drum: Drum):
        self.display.clear()

        for track_index in range(0, 4):
            self.display.set_color(
                Drumpad(
                    track_index), ColorScheme.Tracks[drum.tracks[track_index].note]
            )
            show_track(
                self.display,
                ColorScheme.Tracks[drum.tracks[track_index].note],
                drum.tracks[track_index],
                track_index,
            )

        self.display.show()

    def __read_pots(self, drum: Drum) -> None:
        (speed_changed, speed) = self.speed_pot.read()
        if speed_changed:
            drum.tempo.set_bpm(bpm_from_pot(speed))


def show_track(display, step_color, track, track_index):
    for step_index, step in enumerate(track.sequencer.steps):
        color = None
        if step_index == (track.sequencer.cur_step_index + 7) % 8:
            color = ColorScheme.Cursor
        elif step.active:
            color = step_color

        display.set_color(SequencerKey(step_index, track_index), color)


class PotReader:
    def __init__(self, value_reader, inverted=True):
        self.value_reader = value_reader
        self.inverted = inverted
        self.last_val = None

    def read(self):
        tolerance = 100

        val = self.value_reader()
        if self.last_val is None or abs(val - self.last_val) > tolerance:
            self.last_val = val

            if self.inverted:
                val = POT_MAX - val

            return (True, val)
        else:
            return (False, val)


def bpm_from_pot(pot_value):
    return ((POT_MAX - pot_value) / POT_MAX) * BPM_MAX
