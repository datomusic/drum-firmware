import usb_midi
import adafruit_midi
from adafruit_midi.note_on import NoteOn
from adafruit_midi.timing_clock import TimingClock
from adafruit_midi.midi_continue import Continue

from drum import Drum
from tempo import Tempo

# from adafruit_midi.control_change import ControlChange
from adafruit_midi.note_off import NoteOff

# from adafruit_midi.pitch_bend import PitchBend


quarters = 0
count = 0
midi_ticks = 0
drum = Drum()

drum.tracks[0].note = 60
drum.tracks[1].note = 64
drum.tracks[2].note = 68
drum.tracks[3].note = 72

drum.tracks[0].sequencer.set_step(0)
drum.tracks[0].sequencer.set_step(4)
drum.tracks[1].sequencer.set_step(3)
drum.tracks[1].sequencer.set_step(5)
drum.tracks[2].sequencer.set_step(7)
drum.tracks[3].sequencer.set_step(6)


(midi_in, midi_out) = usb_midi.ports
midi = adafruit_midi.MIDI(midi_in=midi_in, midi_out=midi_out)


class NoteOutput:
    def __init__(self):
        self.ticks = 0
        self.note = 0
        self.active = False

    def play(self, note, vel):
        if self.active:
            midi.send(NoteOff(note))

        self.ticks = 0
        print("NoteOn")
        midi.send(NoteOn(note, vel))
        self.note = note
        self.active = True

    def tick(self):
        if self.active:
            self.ticks += 1
            if self.ticks >= 5:
                self.ticks = 0
                print("NoteOff")
                midi.send(NoteOff(self.note, 0))
                self.active = False


out = NoteOutput()


def play_note(note, vel):
    print(f"Note: {note}, vel: {vel}")
    out.play(note, vel)


def quarter_tick():
    global quarters
    global count
    quarters += 1
    if quarters >= 2:
        quarters = 0
        count += 1
        drum.tick(play_note)


tempo = Tempo()


while True:
    msg = midi.receive()

    if msg is not None:
        if type(msg) is TimingClock:
            tempo.tick()
            midi_ticks += 1
            out.tick()
            if midi_ticks >= 6:
                midi_ticks = 0
                quarter_tick()
        elif type(msg) is Continue:
            print("Reset")
            midi_ticks = 0
            quarters = 0
