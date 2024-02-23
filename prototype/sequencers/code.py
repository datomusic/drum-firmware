import usb_midi
import adafruit_midi
from adafruit_midi.note_on import NoteOn
from adafruit_midi.timing_clock import TimingClock
from adafruit_midi.midi_continue import Continue

from drum import Drum
from tempo import Tempo
from note_output import NoteOutput
from adafruit_midi.note_off import NoteOff


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
    tempo = Tempo(12)

    note_out = NoteOutput(
        lambda note, vel: midi.send(NoteOn(note, vel)),
        lambda note: midi.send(NoteOff(note)),
    )

    def tick_drum():
        def play_note(note, vel):
            note_out.play(note, vel)

        drum.tick(play_note)

    while True:
        msg = midi.receive()

        if msg is not None:
            if type(msg) is TimingClock:
                tempo.tick(tick_drum)
                note_out.tick()

            elif type(msg) is Continue:
                tempo.reset()
                tick_drum()


main()
