from .device_api import Controls
from .controller_api import Controller
from adafruit_midi import MIDI  # type: ignore
from adafruit_midi.timing_clock import TimingClock  # type: ignore
from adafruit_midi.midi_continue import Continue  # type: ignore


class MIDIController(Controller):
    def __init__(self, midi: MIDI):
        self.midi = midi

    def update(self, controls: Controls):
        msg = self.midi.receive()
        while msg:
            if isinstance(msg, TimingClock):
                controls.handle_midi_clock()
            elif isinstance(msg, Continue):
                controls.reset_tempo()
            msg = self.midi.receive()

    def show(self, _drum):
        pass
