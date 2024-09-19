from .drum import Drum
from .tempo import TempoSource
from .settings import Settings
from adafruit_midi import MIDI  # type: ignore
from adafruit_midi.timing_clock import TimingClock  # type: ignore
from adafruit_midi.midi_continue import Continue  # type: ignore
from adafruit_midi.start import Start  # type: ignore
from adafruit_midi.stop import Stop  # type: ignore


class MIDIHandler:
    def __init__(self, midi: MIDI, settings: Settings) -> None:
        self.midi = midi
        self.settings = settings

    def update(self, drum: Drum, delta_ms: int) -> None:
        msg = self.midi.receive()
        while msg:
            if isinstance(msg, TimingClock):
                drum.tempo.tempo_source = TempoSource.MIDI
                drum.tempo.handle_midi_clock()
            elif isinstance(msg, Continue) or isinstance(msg, Start):
                drum.sequencer.set_playing(True)
            elif isinstance(msg, Stop):
                drum.sequencer.set_playing(False)

            msg = self.midi.receive()
