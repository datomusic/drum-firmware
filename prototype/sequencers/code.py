from adafruit_midi.note_off import NoteOff
from note_output import NoteOutput
from tempo import MidiTempo, InternalTempo
from drum import Drum
from adafruit_midi.note_on import NoteOn
import adafruit_midi
import usb_midi
import brains2
from brains2 import ControlName, SequencerKey, KeyboardKey, ControlKey


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


class Colors:
    CurStep = (50, 50, 50)
    Tracks = (
        (200, 0, 0),
        (0, 200, 200),
        (0, 0, 200),
        (200, 0, 200)
    )


def show_sequencer(display, step_color, sequencer):
    for (step_ind, step) in enumerate(sequencer.steps):
        col = None
        if step_ind == sequencer.cur_step_index:
            col = Colors.CurStep
        elif step.active:
            col = step_color

        display.set_color(SequencerKey(step_ind), col)


def main() -> None:
    controls = brains2.Controls()
    display = brains2.Display()
    (midi_in, midi_out) = usb_midi.ports
    midi = adafruit_midi.MIDI(midi_in=midi_in, midi_out=midi_out)
    drum = Drum()
    # tempo = MidiTempo()
    tempo = InternalTempo(100)

    note_out = NoteOutput(
        lambda note, vel: midi.send(NoteOn(note, vel)),
        lambda note: midi.send(NoteOff(note)),
    )

    setup_tracks(drum.tracks)
    current_track = 0

    def on_tick():
        drum.tick(note_out.play)
        note_out.tick()

    def handle_input():
        nonlocal current_track

        event = controls.get_key_event()
        if event:
            key = event.key

            if event.pressed:
                color = (255, 0, 0)
                display.set_color(key, color)

            if isinstance(key, SequencerKey):
                print(f"Seq, step: {key.step}, pressed: {event.pressed}")
                if event.pressed:
                    drum.tracks[current_track].sequencer.toggle_step(key.step)

            elif isinstance(key, KeyboardKey):
                print(
                    f"Keyboard, number: {key.number}, pressed: {event.pressed}"
                )

            elif isinstance(key, ControlKey):
                print(f"Control, name: {key.name}, pressed: {event.pressed}")
                if key.name == ControlName.Seq1:
                    current_track = 0
                elif key.name == ControlName.Seq2:
                    current_track = 1
                elif key.name == ControlName.Down:
                    current_track = 2
                elif key.name == ControlName.Up:
                    current_track = 3

    while True:
        # msg = midi.receive()
        # tempo.handle_message(msg, on_tick)
        if tempo.update():
            on_tick()

        handle_input()

        display.clear()
        show_sequencer(display,
                       Colors.Tracks[current_track],
                       drum.tracks[current_track].sequencer)
        display.show()


main()
