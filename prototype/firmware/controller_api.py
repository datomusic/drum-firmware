from .drum import Drum
from .device_api import Controls, _not_implemented


class Controller:
    def update(self, controls: Controls, delta_ms: int):
        _not_implemented("Controller.update", delta_ms)

    def show(self, drum: Drum):
        _not_implemented("Controller.show")
