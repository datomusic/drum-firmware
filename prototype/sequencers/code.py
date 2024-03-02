from adafruit_midi.note_off import NoteOff
from note_output import NoteOutput
from tempo import MidiTempo
from drum import Drum
from adafruit_midi.note_on import NoteOn
import adafruit_midi
import usb_midi
import brains2


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


def main():
    (midi_in, midi_out) = usb_midi.ports
    midi = adafruit_midi.MIDI(midi_in=midi_in, midi_out=midi_out)
    drum = Drum()
    setup_tracks(drum.tracks)
    tempo = MidiTempo()

    note_out = NoteOutput(
        lambda note, vel: midi.send(NoteOn(note, vel)),
        lambda note: midi.send(NoteOff(note)),
    )
    keys = brains2.Keys()

    def on_tick():
        drum.tick(note_out.play)
        note_out.tick()

    while True:
        msg = midi.receive()
        tempo.handle_message(msg, on_tick)

        event = keys.events.get()
        if event:
            print(event, buttons[event.key_number])


main()
