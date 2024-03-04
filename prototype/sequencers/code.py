from adafruit_midi.note_off import NoteOff
from note_output import NoteOutput
from tempo import MidiTempo, InternalTempo
from drum import Drum
from adafruit_midi.note_on import NoteOn
import adafruit_midi
import usb_midi
from brains2.drum_controls import DrumControls

USE_INTERNAL_TEMPO = False
NOTES_TO_CHANNELS = False # Useful for triggering Volca Drum
ROOT_NOTE = 40

import microcontroller

def setup_tracks(tracks):
    tracks[0].note = 0
    tracks[1].note = 1
    tracks[2].note = 2
    tracks[3].note = 3

    tracks[0].sequencer.set_step(0)
    tracks[0].sequencer.set_step(4)
    tracks[1].sequencer.set_step(3)
    tracks[1].sequencer.set_step(5)
    tracks[2].sequencer.set_step(7)
    tracks[3].sequencer.set_step(6)


def main() -> None:
    drum_controls = DrumControls()
    (midi_in, midi_out) = usb_midi.ports
    midi = adafruit_midi.MIDI(midi_in=midi_in, midi_out=midi_out)
    drum = Drum()
    setup_tracks(drum.tracks)

    tempo: InternalTempo | MidiTempo
    if USE_INTERNAL_TEMPO:
        tempo = InternalTempo(100)
    else:
        tempo = MidiTempo()

    def note_on(note, vel):
        if NOTES_TO_CHANNELS:
            midi.send(NoteOn(1, vel), note)
        else:
            midi.send(NoteOn(ROOT_NOTE + note, vel))

    def note_off(note):
        if NOTES_TO_CHANNELS:
            midi.send(NoteOff(1), note),
        else:
            midi.send(NoteOff(ROOT_NOTE + note))

    note_out = NoteOutput(note_on, note_off)

    def on_tick():
        drum.tick(note_out.play)
        note_out.tick()

    while True:
        if isinstance(tempo, InternalTempo):
            if tempo.update():
                on_tick()

        elif isinstance(tempo, MidiTempo):
            msg = midi.receive()
            tempo.handle_message(msg, on_tick)

        drum_controls.handle_input(drum, note_out)
        drum_controls.show(drum)

main()
