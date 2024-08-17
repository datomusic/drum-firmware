import usb_midi  # type: ignore
from adafruit_midi import MIDI  # type: ignore
from firmware.application import Application
from firmware.midi_output import MIDIOutput
from firmware.audio.audio_output import AudioOutput
from firmware.midi_controller import MIDIController
from firmware.combined_methods import CombinedMethods
from teensy41.pizza_controller import PizzaController
from firmware.device_api import Output
from firmware.controller_api import Controller

(midi_in_port, midi_out_port) = usb_midi.ports

controller = CombinedMethods(
    Controller(),
    [
        PizzaController(track_count=Application.TRACK_COUNT),
        MIDIController(MIDI(midi_in=midi_in_port))
    ]
)

output = CombinedMethods(
    Output(),
    [
        AudioOutput(),
        MIDIOutput(MIDI(midi_out=midi_out_port))
    ]
)

Application(controller, output).run()
