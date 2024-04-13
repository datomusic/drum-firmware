from .device_api import Controller, Output
from .drum import Drum
import adafruit_midi


class PlatformMIDI:
    pass


class MIDIController(Controller):
    def __init__(self, midi: PlatformMIDI):
        self.midi = midi

    def update(self, drum: Drum):
        msg = self.midi.get_message()
        if msg:
            drum.tempo.set_bpm(120)


class MIDIOutput(Output):
    def __init__(self, midi: PlatformMIDI):
        pass
