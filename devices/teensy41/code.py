import usb_midi  # type: ignore
from adafruit_midi import MIDI  # type: ignore
from firmware.application import Application
from firmware.settings import Settings
from firmware.sysex_handler import SysExHandler
from teensy41.pizza_controller import PizzaController
from teensy41.hardware import Teensy41Hardware

settings = Settings()

(midi_in_port, midi_out_port) = usb_midi.ports
midi = MIDI(midi_in=midi_in_port, midi_out=midi_out_port)
        MIDI(midi_in=midi_in_port),
        settings=settings,
        sysex_handler=SysExHandler(settings)
    )

controller = PizzaController(settings, Teensy41Hardware())
Application(controller, midi, settings).run()
