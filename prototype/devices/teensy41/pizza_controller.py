from firmware.device_api import Controller
from firmware.drum import Drum

from .hardware import (
    Controls,
    PotName,
)

BPM_MAX = 500
POT_MIN = 0
POT_MAX = 65536


class PizzaController(Controller):
    def __init__(self):
        self.controls = Controls()
        self.speed_pot = PotReader(
            lambda: self.controls.read_pot(PotName.Speed))

    def update(self, drum: Drum) -> None:
        self.__read_pots(drum)

    def __read_pots(self, drum: Drum) -> None:
        (speed_changed, speed) = self.speed_pot.read()
        if speed_changed:
            drum.tempo.set_bpm(bpm_from_pot(speed))


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
