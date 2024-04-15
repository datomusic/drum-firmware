from .device_api import Output
from adafruit_midi import MIDI  # type: ignore
from adafruit_midi.control_change import ControlChange  # type: ignore
from adafruit_midi.note_on import NoteOn  # type: ignore
from adafruit_midi.note_off import NoteOff  # type: ignore


class MIDIOutput(Output):
    def __init__(self, midi: MIDI):
        self.midi = midi

    def set_filter(self, amount_percent) -> None:
        self._print_send(ControlChange(74, percent_to_midi(amount_percent)))

    def send_note_on(self, channel: int, note: int, vel_percent: float):
        self.midi.send(NoteOn(note, percent_to_midi(vel_percent)), channel=channel)

    def send_note_off(self, channel: int, note: int):
        self.midi.send(NoteOff(note), channel=channel)

    def set_channel_pitch(self, channel: int, pitch_percent: float):
        self._print_send(
            ControlChange(16 + channel, percent_to_midi(pitch_percent))
        )

    def set_volume(self, vol_percent):
        self._print_send(ControlChange(7, percent_to_midi(vol_percent)))

    def _print_send(self, msg):
        print(msg)
        self.midi.send(msg)


def percent_to_midi(percent):
    return max(0, min(int((percent * 127) / 100), 127))
