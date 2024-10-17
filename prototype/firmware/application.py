import time
import gc
import firmware.metrics as metrics
from .settings import Settings
from .midi_output import MIDIOutput
from .controller_api import Controller
from .drum import Drum
from .midi_handler import MIDIHandler
from adafruit_midi import MIDI
import adafruit_logging as logging
from .output_api import Output
from .broadcaster import Broadcaster

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)  # type: ignore

metrics.PRINT_REPORT = True
metrics.WITH_MEMORY_METRICS = False


class Application:
    def __init__(self,
                 controller: Controller,
                 device_output: Output,
                 midi: MIDI,
                 settings: Settings) -> None:

        self.settings = settings
        self.controller = controller
        self.midi_handler = MIDIHandler(midi, settings)

        midi_output = MIDIOutput(midi)
        combined_output = Broadcaster(Output, [device_output, midi_output])
        self.drum = Drum(combined_output, settings)  # type: ignore

        self._metrics = metrics.Metrics()
        self._gc_collect = self._metrics.wrap("gc_collect", gc.collect)
        self._fast_update = self._metrics.wrap("fast_update", self.fast_update)
        self._slow_update = self._metrics.wrap("slow_update", self.slow_update)
        self._show = self._metrics.wrap("show", self.show)

        self._last_nanoseconds = time.monotonic_ns()
        self._loop_counter = 0
        self._accumulated_show_ns = 0
        self._accumulated_slow_update_ns = 0

        gc.disable()
        logging._default_handler.setLevel(logging.DEBUG)  # type: ignore
        logger.info("Application initialized")

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

    def run(self):
        logger.info("Application running")
        while True:
            self.loop_step()

    def slow_update(self, delta_milliseconds: int) -> None:
        self.controller.update(self.drum, delta_milliseconds)

    def fast_update(self, delta_nanoseconds: int) -> None:
        delta_milliseconds = delta_nanoseconds // 1_000_000
        self.midi_handler.update(self.drum, delta_milliseconds)
        self.drum.tempo.update(delta_nanoseconds)

        self.controller.fast_update(self.drum, delta_milliseconds)

    def show(self, delta_milliseconds) -> None:
        self.controller.show(
            self.drum, delta_milliseconds, self.drum.tempo.get_beat_position()
        )
