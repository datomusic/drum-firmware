import usb_midi  # type: ignore
from adafruit_midi import MIDI  # type: ignore
from firmware.application import Application
from firmware.settings import Settings
from firmware.audio.audio_output import AudioOutput
from firmware.broadcaster import Broadcaster
from teensy41.pizza_controller import PizzaController
from teensy41.hardware import Teensy41Hardware
from firmware.controller_api import Controller

settings = Settings()
    [
        PizzaController(
            track_count=Application.TRACK_COUNT,
            hardware=Teensy41Hardware(using_PWM=True)
        ),
    ]
)

output = Broadcaster(
    Output(),
    [
        AudioOutput(),
        MIDIOutput(MIDI(midi_out=midi_out_port))
    ]
)

controller = PizzaController(settings, Teensy41Hardware())
Application(controller, midi, settings).run()
