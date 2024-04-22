from .device_api import Output, OutputParam
from adafruit_midi import MIDI  # type: ignore
from adafruit_midi.control_change import ControlChange  # type: ignore
from adafruit_midi.note_on import NoteOn  # type: ignore
from adafruit_midi.note_off import NoteOff  # type: ignore


class MIDIOutput(Output):
    def __init__(self, midi: MIDI):
        self.midi = midi
        self.filter_amount = 50

    def send_note_on(self, channel: int, note: int, vel_percent: float):
        self.midi.send(
            NoteOn(note, percent_to_midi(vel_percent)), channel=channel)

    def send_note_off(self, channel: int, note: int):
        self.midi.send(NoteOff(note), channel=channel)

    def set_channel_pitch(self, channel: int, pitch_percent: float):
        self.send_cc(16 + channel, percent_to_midi(pitch_percent))

    def set_param(self, param, value) -> None:
        if param == OutputParam.Volume:
            self.send_cc(7, percent_to_midi(value))

        elif param == OutputParam.LowPass:
            self.send_cc(75, percent_to_midi(value))

        elif param == OutputParam.HighPass:
            self.send_cc(76, percent_to_midi(value))

        elif param == OutputParam.AdjustFilter:
            self.filter_amount = constrain_midi(
                int(self.filter_amount + value))
            self.send_cc(74, self.filter_amount)

    def send_cc(self, cc, value, channel=1):
        print(f"[{channel}] CC {cc}: {value}")
        self.midi.send(ControlChange(cc, value), channel=channel)


def constrain_midi(value):
    return max(0, min(127, value))


def percent_to_midi(percent):
    return constrain_midi(round((percent * 127) / 100))
