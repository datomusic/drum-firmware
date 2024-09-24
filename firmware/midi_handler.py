import adafruit_logging as logging
from .drum import Drum
from .tempo import TempoSource
from .settings import Settings
from .sysex_handler import SysexHandler
from adafruit_midi import MIDI
from adafruit_midi.timing_clock import TimingClock
from adafruit_midi.midi_continue import Continue
from adafruit_midi.start import Start
from adafruit_midi.stop import Stop
from adafruit_midi.system_exclusive import SystemExclusive

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)  # type: ignore

# TODO: Use real manufacturer ID
__MANUFACTURER_ID = bytes([0x7D])


class MIDIHandler:
    def __init__(self, midi: MIDI, settings: Settings) -> None:
        self.midi = midi
        self.settings = settings
        self.sysex_handler = SysexHandler(self._send_sysex_data, settings)
        logger.debug("MIDIHandler initialized")

    def update(self, drum: Drum, delta_ms: int) -> None:
        message = self.midi.receive()
        while message:
            if isinstance(message, TimingClock):
                drum.tempo.tempo_source = TempoSource.MIDI
                drum.tempo.handle_midi_clock()
            elif isinstance(message, Continue) or isinstance(message, Start):
                drum.sequencer.set_playing(True)
            elif isinstance(message, Stop):
                drum.sequencer.set_playing(False)
            elif isinstance(message, SystemExclusive):
                if message.manufacturer_id != __MANUFACTURER_ID:
                    logger.error("Invalid manufacturer ID")
                else:
                    self.sysex_handler.handle_sysex_data(message)

            message = self.midi.receive()

    def _send_sysex_data(self, data: bytes) -> None:
        self.midi.send(SystemExclusive(__MANUFACTURER_ID, data))
