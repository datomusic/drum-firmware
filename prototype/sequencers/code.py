from adafruit_midi.note_off import NoteOff
from note_output import NoteOutput
from tempo import MidiTempo
from drum import Drum
from adafruit_midi.note_on import NoteOn
import adafruit_midi
import usb_midi
import brains2
from brains2 import KeyEvent, ControlName, SequencerKey, KeyboardKey, ControlKey


def setup_tracks(tracks):
    tracks[0].note = 60
    tracks[1].note = 64
    tracks[2].note = 68
    tracks[3].note = 72

    tracks[0].sequencer.set_step(0)
    tracks[0].sequencer.set_step(4)
    tracks[1].sequencer.set_step(3)
    tracks[1].sequencer.set_step(5)
    tracks[2].sequencer.set_step(7)
    tracks[3].sequencer.set_step(6)


def main() -> None:
    keys = brains2.Controls()
    display = brains2.Display()
    (midi_in, midi_out) = usb_midi.ports
    midi = adafruit_midi.MIDI(midi_in=midi_in, midi_out=midi_out)
    drum = Drum()
    tempo = MidiTempo()

    note_out = NoteOutput(
        lambda note, vel: midi.send(NoteOn(note, vel)),
        lambda note: midi.send(NoteOff(note)),
    )

    setup_tracks(drum.tracks)

    def on_tick():
        drum.tick(note_out.play)
        note_out.tick()

    while True:
        msg = midi.receive()
        tempo.handle_message(msg, on_tick)

        event = keys.get_event()
        if isinstance(event, KeyEvent):
            col = None

            if event.pressed:
                col = (255, 0, 0)

            key = event.key
            display.set_color(key, col)
            if isinstance(key, SequencerKey):
                print(f"Seq, step: {key.step}, pressed: {event.pressed}")

            elif isinstance(key, KeyboardKey):
                print(
                    f"Keyboard, number: {key.number}, pressed: {event.pressed}")

            elif isinstance(key, ControlKey):
                print(f"Control, name: {key.name}, pressed: {event.pressed}")


main()
