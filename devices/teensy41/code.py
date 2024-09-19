import usb_midi  # type: ignore
from adafruit_midi import MIDI  # type: ignore
from firmware.application import Application
from firmware.settings import Settings
from teensy41.pizza_controller import PizzaController
from teensy41.hardware import Teensy41Hardware

settings = Settings()

(midi_in_port, midi_out_port) = usb_midi.ports
midi = MIDI(midi_in=midi_in_port, midiout=midi_out_port)

controller = PizzaController(settings, Teensy41Hardware())
Application(controller, midi, settings).run()
