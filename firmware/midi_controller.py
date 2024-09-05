from .device_api import Controls
from .settings import Settings
from .controller_api import Controller
from adafruit_midi import MIDI  # type: ignore
from adafruit_midi.timing_clock import TimingClock  # type: ignore
from adafruit_midi.midi_continue import Continue  # type: ignore
from adafruit_midi.start import Start  # type: ignore
from adafruit_midi.stop import Stop  # type: ignore


class MIDIController(Controller):
    def __init__(self, midi: MIDI, settings: Settings):
        self.midi = midi
        self.settings = settings

    def fast_update(self, controls: Controls, delta_ms: int):
        pass

    def update(self, controls: Controls, delta_ms: int):
        msg = self.midi.receive()
        while msg:
            if isinstance(msg, TimingClock):
                controls.handle_midi_clock()
            elif isinstance(msg, Continue) or isinstance(msg, Start):
                controls.set_playing(True)
            elif isinstance(msg, Stop):
                controls.set_playing(False)

            msg = self.midi.receive()

    def show(self, _drum, _delta_ms, _beat_position):
        pass

    def on_track_sample_played(self, track_index: int):
        pass
