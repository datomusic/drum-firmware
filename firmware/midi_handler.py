from .drum import Drum
from .tempo import TempoSource
from .settings import Settings
from .sysex_handler import SysexHandler
from adafruit_midi import MIDI, MIDIMessage  # type: ignore
from adafruit_midi.timing_clock import TimingClock  # type: ignore
from adafruit_midi.midi_continue import Continue  # type: ignore
from adafruit_midi.start import Start  # type: ignore
from adafruit_midi.stop import Stop  # type: ignore
from adafruit_midi.system_exclusive import SystemExclusive  # type: ignore


class MIDIHandler:
    def __init__(self, midi: MIDI, settings: Settings) -> None:
        self.midi = midi
        self.settings = settings
        self.sysex_handler = SysexHandler(self._send_message, settings)

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
                self.sysex_handler.handle_sysex_data(message)

            message = self.midi.receive()

    def _send_message(self, message: MIDIMessage) -> None:
        self.midi.send(message)
