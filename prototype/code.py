from firmware.application import Application

# from devices.brains2.device import Brains2Device

# (midi_in, midi_out) = usb_midi.ports
# midi = adafruit_midi.MIDI(midi_in=midi_in, midi_out=midi_out)

from firmware.midi_controller import MIDIController, MIDIOutput, PlatformMIDI
from devices.teensy41.pizza_controller import PizzaController


class DummyMIDI(PlatformMIDI):
    def __init__(self):
        pass

    def get_message(self):
        return None


midi = DummyMIDI()
midi_output = MIDIOutput(midi)

controllers = [MIDIController(midi), PizzaController()]

Application(controllers, midi_output).run()
