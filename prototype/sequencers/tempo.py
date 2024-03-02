from adafruit_midi.timing_clock import TimingClock
from adafruit_midi.midi_continue import Continue


class Tempo:
    def __init__(self, ticks_per_beat):
        self.ticks_per_beat = ticks_per_beat
        self.ticks = 0

    def tick(self, on_beat):
        if self.ticks % self.ticks_per_beat == 0:
            self.ticks = 0
            on_beat()
        self.ticks += 1

    def reset(self):
        self.ticks = 0


class MidiTempo:
    def __init__(self):
        self.temp = Tempo(12)

    def handle_message(self, msg, on_tick):
        if type(msg) is TimingClock:
            self.tempo.tick(on_tick)

        elif type(msg) is Continue:
            self.tempo.reset()
            on_tick()
