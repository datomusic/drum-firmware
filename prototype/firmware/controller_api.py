from .drum import Drum
from .output_api import _not_implemented


class Controller:
    def fast_update(self, drum: Drum, delta_ms: int):
        _not_implemented("Controller.update", delta_ms)

    def update(self, drum: Drum, delta_ms: int):
        _not_implemented("Controller.update", delta_ms)

    def show(self, drum: Drum, delta_ms: int, beat_position: float):
        _not_implemented("Controller.show", beat_position)
