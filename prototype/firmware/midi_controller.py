from .device_api import Controller, Output
from .drum import Drum
from adafruit_midi import MIDI  # type: ignore
from adafruit_midi.control_change import ControlChange  # type: ignore


class PlatformMIDI:
    def get_message(self):
        return None


class MIDIController(Controller):
    def __init__(self, midi: PlatformMIDI):
        self.midi = midi

    def update(self, drum: Drum, output: Output):
        msg = self.midi.get_message()
        if msg:
            drum.tempo.set_bpm(120)


class MIDIOutput(Output):
    def __init__(self, midi: MIDI):
        self.midi = midi
        self.filter = 64

    def adjust_filter(self, value):
        self.filter = max(0, min(self.filter + value, 127))
        print("Filter:", self.filter)
        self.midi.send(ControlChange(74, self.filter))

    def send_note_on(self, note, vel):
        pass

    def send_note_off(self, note):
        pass
