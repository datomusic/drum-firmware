import usb_midi  # type: ignore
from adafruit_midi import MIDI  # type: ignore
from firmware.application import Application
from firmware.midi_output import MIDIOutput
from firmware.audio_output import AudioOutput
from firmware.midi_controller import MIDIController
from teensy41.pizza_controller import PizzaController
from firmware.device_api import Output

(midi_in_port, midi_out_port) = usb_midi.ports


controllers = [
    PizzaController(track_count=Application.TRACK_COUNT),
    MIDIController(MIDI(midi_in=midi_in_port)),
]


class CombinedOutput(Output):
    def __init__(self, outputs: [Output]):
        self.outputs = outputs

    def send_note_on(self, channel: int, note: int, vel_percent: float):
        for output in self.outputs:
            output.send_note_on(channel, note, vel_percent)

    def send_note_off(self, channel: int, note: int):
        for output in self.outputs:
            output.send_note_off(channel, note)

    def set_channel_pitch(self, channel: int, pitch_percent: float):
        for output in self.outputs:
            output.set_channel_pitch(channel, pitch_percent)

    def set_channel_mute(self, channel: int, pressure: float):
        for output in self.outputs:
            output.set_channel_mute(channel, pressure)

    def set_param(self, param, value) -> None:
        for output in self.outputs:
            output.set_param(param, value)

    def on_tempo_tick(self, source) -> None:
        for output in self.outputs:
            output.on_tempo_tick(source)


output = CombinedOutput([AudioOutput(), MIDIOutput(MIDI(midi_out=midi_out_port))])
Application(controllers, output).run()
