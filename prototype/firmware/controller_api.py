from .drum import Drum
from .device_api import Controls, _not_implemented


class Controller:
    def update(self, controls: Controls, delta_ms: int):
        _not_implemented("Controller.update", delta_ms)

    def show(self, drum: Drum):
        _not_implemented("Controller.show")

    def on_track_sample_played(self, track_index: int):
        _not_implemented("Controller.on_track_sample_played", track_index)
