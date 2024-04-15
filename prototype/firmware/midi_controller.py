from .device_api import Controls
from .controller_api import Controller
from adafruit_midi import MIDI  # type: ignore


class MIDIController(Controller):
    def __init__(self, midi: MIDI):
        self.midi = midi

    def update(self, controls: Controls):
        # TODO: Handle incoming MIDI
        pass
        # msg = self.midi.get_message()
        # if msg:
        #     controls.set_bpm(120)
