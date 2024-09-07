import time
import gc
from .settings import Settings
from .output_api import Output
from .controller_api import Controller
from .drum import Drum
from .metrics import Metrics


class Application:
    def __init__(
        self, controllers: list[Controller], output: Output, settings: Settings
    ) -> None:
        self.settings = settings
        self.controllers = controllers
        self.output = output
        self.drum = Drum(self.output, settings)

        self._metrics = Metrics()
        self._gc_collect = self._metrics.wrap("gc_collect", gc.collect)
        self._fast_update = self._metrics.wrap("fast_update", self.fast_update)
        self._slow_update = self._metrics.wrap("slow_update", self.slow_update)
        self._show = self._metrics.wrap("show", self.show)

        self._last_nanoseconds = time.monotonic_ns()
        self._loop_counter = 0
        self._accumulated_show_ns = 0
        self._accumulated_slow_update_ns = 0
        gc.disable()

    def slow_update(self, delta_milliseconds: int) -> None:
        for controller in self.controllers:
            controller.update(self.drum, delta_milliseconds)

    def fast_update(self, delta_nanoseconds: int) -> None:
        self.drum.tempo.update(delta_nanoseconds)

        delta_milliseconds = delta_nanoseconds // 1_000_000
        for controller in self.controllers:
            controller.fast_update(self.drum, delta_milliseconds)

    def show(self, delta_milliseconds) -> None:
        for controller in self.controllers:
            controller.show(self.drum, delta_milliseconds,
                            self.drum.tempo.get_beat_position())

    def run(self):
        while True:
            self.loop_step()

    def loop_step(self) -> None:
        self._metrics.begin_loop()

        now = time.monotonic_ns()
        delta_nanoseconds = now - self._last_nanoseconds
        self._last_nanoseconds = now

        self._accumulated_show_ns += delta_nanoseconds
        self._accumulated_slow_update_ns += delta_nanoseconds

        self._fast_update(delta_nanoseconds)

        if self._loop_counter % 2 == 0:
            ms = self._accumulated_slow_update_ns // 1_000_000
            self._slow_update(ms)
            self._accumulated_slow_update_ns -= ms * 1_000

        if self._accumulated_show_ns > 30_000_000:
            self._show(self._accumulated_show_ns // 1_000_000)
            self._accumulated_show_ns = 0
        else:
            self._gc_collect()

        self._loop_counter += 1
        self._metrics.end_loop(delta_nanoseconds)


