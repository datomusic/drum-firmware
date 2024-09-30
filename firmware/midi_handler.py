import adafruit_logging as logging
from .drum import Drum
from .tempo import TempoSource
from .settings import Settings
from .device_protocol import DeviceProtocol
from adafruit_midi import MIDI
from adafruit_midi.timing_clock import TimingClock
from adafruit_midi.midi_continue import Continue
from adafruit_midi.start import Start
from adafruit_midi.stop import Stop
from adafruit_midi.system_exclusive import SystemExclusive

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)  # type: ignore

# DATO SysEx ID
__MANUFACTURER_ID = bytes([0, 0x22, 0x01])


class MIDIHandler:
    def __init__(self, midi: MIDI, settings: Settings) -> None:
        self.midi = midi
        self.settings = settings
        self.device_protocol = DeviceProtocol(self._send_sysex_data, settings)
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
                    logger.error(f"Invalid manufacturer ID. Got {message.manufacturer_id}")
                else:
                    self.device_protocol.handle_message(message.data)

            message = self.midi.receive()

    def _send_sysex_data(self, data: bytes) -> None:
        self.midi.send(SystemExclusive(__MANUFACTURER_ID, data))
