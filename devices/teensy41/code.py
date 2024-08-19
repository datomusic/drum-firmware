import usb_midi  # type: ignore
from adafruit_midi import MIDI  # type: ignore
from firmware.application import Application
from firmware.midi_output import MIDIOutput
from firmware.audio.audio_output import AudioOutput
from firmware.midi_controller import MIDIController
from firmware.broadcaster import Broadcaster
from teensy41.pizza_controller import PizzaController
from teensy41.hardware import Teensy41Hardware
from firmware.device_api import Output
from firmware.controller_api import Controller

(midi_in_port, midi_out_port) = usb_midi.ports

controller = Broadcaster(
    Controller(),
    [
        PizzaController(
            track_count=Application.TRACK_COUNT,
            hardware=Teensy41Hardware(using_PWM=True)
        ),
        MIDIController(MIDI(midi_in=midi_in_port))
    ]
)

output = Broadcaster(
    Output(),
    [
        AudioOutput(),
        MIDIOutput(MIDI(midi_out=midi_out_port))
    ]
)

Application(controller, output).run()
