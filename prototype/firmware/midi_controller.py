from .device_api import Controller, Controls, Output
from adafruit_midi import MIDI  # type: ignore
from adafruit_midi.control_change import ControlChange  # type: ignore


class PlatformMIDI:
    def get_message(self):
        return None


class MIDIController(Controller):
    def __init__(self, midi: PlatformMIDI):
        self.midi = midi

    def update(self, controls: Controls):
        msg = self.midi.get_message()
        if msg:
            controls.set_bpm(120)


class MIDIOutput(Output):
    def __init__(self, midi: MIDI):
        self.midi = midi

    def set_filter(self, value):
        print("Filter:", value)
        self.midi.send(ControlChange(74, value))

    def send_note_on(self, note, vel):
        pass

    def send_note_off(self, note):
        pass
