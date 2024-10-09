from .tempo import TempoSource
from .output_api import Output, OutputParam, OutputChannelParam
from adafruit_midi import MIDI
from adafruit_midi.control_change import ControlChange
from adafruit_midi.note_on import NoteOn
from adafruit_midi.note_off import NoteOff
from adafruit_midi.timing_clock import TimingClock
from adafruit_midi.channel_pressure import ChannelPressure


class MIDIOutput(Output):
    def __init__(self, midi: MIDI):
        self.midi = midi
        self.filter_amount = 64

    def send_note_on(self, channel: int, note: int, vel_percent: float):
        self.midi.send(NoteOn(note, percent_to_midi(vel_percent)), channel=channel)

    def send_note_off(self, channel: int, note: int):
        self.midi.send(NoteOff(note), channel=channel)

    def set_channel_param(self, channel: int, param, value_percent: float):
        if param == OutputChannelParam.Pitch:
            self._send_cc(16 + channel, percent_to_midi(value_percent))
        elif param == OutputChannelParam.Mute:
            midi_value = percent_to_midi(value_percent)
            self.midi.send(ChannelPressure(midi_value), channel=channel)
        else:
            raise TypeError(f"Invalid output channel parameter: {param}")

    def set_param(self, param, value) -> None:
        pass

        # if param == OutputParam.Volume:
        #     self._send_cc(7, percent_to_midi(value))

        # elif param == OutputParam.Tempo:
        #     self._send_cc(3, percent_to_midi(value))

        # elif param == OutputParam.LowPass:
        #     self._send_cc(75, percent_to_midi(value))

        # elif param == OutputParam.HighPass:
        #     self._send_cc(76, percent_to_midi(value))

        # elif param == OutputParam.AdjustFilter:
        #     self.filter_amount = constrain_midi(int(self.filter_amount + value))
        #     self._send_cc(74, self.filter_amount)

        # elif param == OutputParam.Distortion:
        #     self._send_cc(77, percent_to_midi(value))

        # elif param == OutputParam.Bitcrusher:
        #     self._send_cc(78, percent_to_midi(value))

    def on_tempo_tick(self, source) -> None:
        if source == TempoSource.Internal:
            self.midi.send(TimingClock())

    def _send_cc(self, cc, value, channel=0):
        print(f"[{channel}] CC {cc}: {value}")
        self.midi.send(ControlChange(cc, value), channel=channel)


def constrain_midi(value):
    return max(0, min(127, value))


def percent_to_midi(percent):
    return constrain_midi(round((percent * 127) / 100))
