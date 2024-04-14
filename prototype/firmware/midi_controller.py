from .device_api import Controls, Output
from .controller_api import Controller
from adafruit_midi import MIDI  # type: ignore
from adafruit_midi.control_change import ControlChange  # type: ignore
from adafruit_midi.note_on import NoteOn  # type: ignore
from adafruit_midi.note_off import NoteOff  # type: ignore


class PlatformMIDI:
    def get_message(self):
        return None


class MIDIController(Controller):
    def __init__(self, midi: PlatformMIDI):
        self.midi = midi

    def update(self, controls: Controls):
        # TODO: Handle incoming MIDI
        pass
        # msg = self.midi.get_message()
        # if msg:
        #     controls.set_bpm(120)


class MIDIOutput(Output):
    def __init__(self, midi: MIDI):
        self.midi = midi

    def set_filter(self, value):
        print("Filter:", value)
        self.midi.send(ControlChange(74, value))

    def send_note_on(self, channel: int, note: int, vel_percent: float):
        velocity = int((vel_percent * 127) / 100)
        self.midi.send(NoteOn(note, velocity), channel)

    def send_note_off(self, channel: int, note: int):
        self.midi.send(NoteOff(note), channel)
