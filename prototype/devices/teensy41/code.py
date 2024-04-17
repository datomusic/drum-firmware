import usb_midi  # type: ignore
from adafruit_midi import MIDI  # type: ignore
from firmware.application import Application
from firmware.midi_output import MIDIOutput
from teensy41.pizza_controller import PizzaController

(midi_in, midi_out) = usb_midi.ports
midi = MIDI(midi_in=midi_in, midi_out=midi_out)

controllers = [PizzaController()]
output = MIDIOutput(midi)

Application(controllers, output).run()
