from note_output import NoteOutput

from adafruit_midi.note_off import NoteOff
from adafruit_midi.note_on import NoteOn
import adafruit_midi
import usb_midi

NOTES_TO_CHANNELS = False  # Useful for triggering Volca Drum
ROOT_NOTE = 0


def open_midi():
    (midi_in, midi_out) = usb_midi.ports
    return adafruit_midi.MIDI(midi_in=midi_in, midi_out=midi_out)


def get_midi_note_out(midi):
    def note_on(note, vel):
        if NOTES_TO_CHANNELS:
            midi.send(NoteOn(1, vel), note)
        else:
            midi.send(NoteOn(ROOT_NOTE + note, vel))

    def note_off(note):
        if NOTES_TO_CHANNELS:
            midi.send(NoteOff(1), note)
        else:
            midi.send(NoteOff(ROOT_NOTE + note))

    return NoteOutput(note_on, note_off)
