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

    def slow_update(self, delta_ms: int) -> None:
        for controller in self.controllers:
            controller.update(self.drum, delta_ms)

    def fast_update(self, delta_ns: int) -> None:
        self.drum.tempo.update(delta_ns)

        delta_ms = delta_ns // 1_000_000
        for controller in self.controllers:
            controller.fast_update(self.drum, delta_ms)

    def show(self, delta_ms) -> None:
        for controller in self.controllers:
            controller.show(self.drum, delta_ms,
                            self.drum.tempo.get_beat_position())

    def run(self):
        for _ in self.run_iterator():
            pass

    def run_iterator(self) -> object:
        gc.disable()
        metrics = Metrics()

        accumulated_show_ns = 0
        accumulated_slow_update_ns = 0
        loop_counter = 0

        gc_collect = metrics.wrap("gc_collect", gc.collect)
        fast_update = metrics.wrap("fast_update", self.fast_update)
        slow_update = metrics.wrap("slow_update", self.slow_update)
        show = metrics.wrap("show", self.show)

        last_nanoseconds = time.monotonic_ns()

        while True:
            metrics.begin_loop()

            now = time.monotonic_ns()
            delta_nanoseconds = now - last_nanoseconds
            last_nanoseconds = now

            accumulated_show_ns += delta_nanoseconds
            accumulated_slow_update_ns += delta_nanoseconds

            fast_update(delta_nanoseconds)

            if loop_counter % 2 == 0:
                ms = accumulated_slow_update_ns // 1_000_000
                slow_update(ms)
                accumulated_slow_update_ns -= ms * 1_000

            if accumulated_show_ns > 30_000_000:
                show(accumulated_show_ns // 1_000_000)
                accumulated_show_ns = 0
            else:
                gc_collect()

            loop_counter += 1
            metrics.end_loop(delta_nanoseconds)
            yield


