import usb_midi  # type: ignore
from adafruit_midi import MIDI  # type: ignore
from firmware.application import Application
from firmware.settings import Settings
from pico2.pizza_controller import PizzaController
from pico2.hardware import Pico2Hardware

settings = Settings()

(midi_in_port, midi_out_port) = usb_midi.ports
midi = MIDI(midi_in=midi_in_port, midi_out=midi_out_port)
controller = PizzaController(settings, Pico2Hardware())

Application(controller, midi, settings).run()
