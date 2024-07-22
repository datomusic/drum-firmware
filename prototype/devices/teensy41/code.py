import usb_midi  # type: ignore
from adafruit_midi import MIDI  # type: ignore
from firmware.application import Application
from firmware.midi_output import MIDIOutput
from firmware.audio_output import AudioOutput
from firmware.midi_controller import MIDIController
from teensy41.pizza_controller import PizzaController

(midi_in_port, midi_out_port) = usb_midi.ports


controllers = [
    PizzaController(track_count=Application.TRACK_COUNT),
    MIDIController(MIDI(midi_in=midi_in_port))]

# output = MIDIOutput(MIDI(midi_out=midi_out_port))
output = AudioOutput()

Application(controllers, output).run()
