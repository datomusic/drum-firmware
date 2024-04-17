import usb_midi  # type: ignore
from adafruit_midi import MIDI  # type: ignore
from firmware.application import Application
from firmware.midi_output import MIDIOutput
from teensy41.pizza_controller import PizzaController
from firmware.print_output import PrintOutput
from teensy41.hardware import Teensy41Hardware, DefaultPins

use_valter_pins = True

if not use_valter_pins:
    pins = DefaultPins()
else:
    import board

    class ValterPins:
        def __init__(self):
            self.repeat_button = board.A0
            self.pitch1 = board.A1

            self.drum_pad1 = board.A2
            self.drum_pad1_bottom = board.A3

            self.volume_pot = board.A4
            self.swing_right = board.D8
            self.swing_left = board.D7
            self.pitch2 = board.A7

            self.drum_pad2 = board.A8
            self.drum_pad2_bottom = board.A9

            self.random_button = board.A10
            self.pitch3 = board.A11

            self.drum_pad3 = board.A12
            self.drum_pad3_bottom = board.A13

            self.play_button = board.D37
            self.speed_pot = board.D38
            self.filter_right = board.A5
            self.filter_left = board.A6
            self.pitch4 = board.D39

            self.drum_pad4 = board.D40
            self.drum_pad4_bottom = board.D41

    pins = ValterPins()


(midi_in, midi_out) = usb_midi.ports
midi = MIDI(midi_in=midi_in, midi_out=midi_out)

controllers = [PizzaController(Teensy41Hardware(pins))]
# output = MIDIOutput(midi)
output = PrintOutput()

Application(controllers, output).run()
