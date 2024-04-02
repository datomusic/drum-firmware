from adafruit_midi.note_off import NoteOff
from note_output import NoteOutput
from adafruit_midi.note_on import NoteOn
from tempo import Tempo
from drum import Drum
import adafruit_midi
import usb_midi

USE_INTERNAL_TEMPO = True
NOTES_TO_CHANNELS = False  # Useful for triggering Volca Drum
ROOT_NOTE = 40


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


def make_note_out(midi):
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

    return NoteOutput(note_on, note_off)


def run_application(device):
    (midi_in, midi_out) = usb_midi.ports
    midi = adafruit_midi.MIDI(midi_in=midi_in, midi_out=midi_out)
    drum = Drum()
    setup_tracks(drum.tracks)
    note_out = make_note_out(midi)

    def on_tempo_tick():
        drum.tick(note_out.play)
        note_out.tick()

    tempo = Tempo(midi, on_tempo_tick)
    tempo.use_internal = USE_INTERNAL_TEMPO

    while True:
        msg = midi.receive()
        if msg:
            tempo.on_midi_msg(msg)

        tempo.update()
        device.handle_input(drum, note_out)
        device.show(drum)
