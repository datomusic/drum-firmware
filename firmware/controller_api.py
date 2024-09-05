from .drum import Drum
from .device_api import _not_implemented


class Controller:
    def fast_update(self, drum: Drum, delta_ms: int):
        _not_implemented("Controller.update", delta_ms)

    def update(self, drum: Drum, delta_ms: int):
        _not_implemented("Controller.update", delta_ms)

    def show(self, drum: Drum, delta_ms: int, beat_position: float):
        _not_implemented("Controller.show", beat_position)

    def on_track_sample_played(self, track_index: int):
        _not_implemented("Controller.on_track_sample_played", track_index)
