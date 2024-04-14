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


def percent_to_midi(percent):
    return int((percent * 127) / 100)


class MIDIOutput(Output):
    def __init__(self, midi: MIDI):
        self.midi = midi

    def set_filter(self, channel, value) -> None:
        print("Filter:", value)
        self._send_msg(ControlChange(74, value), channel)

    def send_note_on(self, channel: int, note: int, vel_percent: float):
        self._send_msg(NoteOn(note, percent_to_midi(vel_percent)), channel)

    def send_note_off(self, channel: int, note: int):
        self._send_msg(NoteOff(note), channel)

    def set_channel_pitch(self, channel: int, pitch_percent: float):
        print(f"pitch_percent: {pitch_percent}")
        self._send_msg(ControlChange(
            16, percent_to_midi(pitch_percent)), channel)

    def _send_msg(self, msg, channel):
        print(f"{msg}, channel: {channel}")
        self.midi.send(msg, channel)
