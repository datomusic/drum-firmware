import usb_midi  # type: ignore
from adafruit_midi import MIDI  # type: ignore
from firmware.application import Application
from firmware.settings import Settings
from firmware.midi_output import MIDIOutput
from firmware.midi_controller import MIDIController
from teensy41.pizza_controller import PizzaController
from teensy41.hardware import Teensy41Hardware

settings = Settings()
(midi_in_port, midi_out_port) = usb_midi.ports

controllers = [
    PizzaController(
        track_count=Application.TRACK_COUNT,
        hardware=Teensy41Hardware(),
        settings=settings,
    ),
    MIDIController(MIDI(midi_in=midi_in_port), settings=settings),
]

output = MIDIOutput(MIDI(midi_out=midi_out_port))

Application(controllers, output, settings).run()
