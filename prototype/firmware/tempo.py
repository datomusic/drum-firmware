from adafruit_midi.timing_clock import TimingClock
from adafruit_midi.midi_continue import Continue
import time


class BeatTicks:
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
        self.ticks = BeatTicks(12)

    def handle_message(self, msg, on_tick):
        if type(msg) is TimingClock:
            self.ticks.tick(on_tick)

        elif type(msg) is Continue:
            self.ticks.reset()
            on_tick()


class InternalTempo:
    def __init__(self, bpm):
        self.acc = 0
        self.last = time.monotonic_ns()
        self.set_bpm(bpm)

    def set_bpm(self, bpm):
        if bpm < 1:
            bpm = 1
        bpm = int(bpm)
        print(f"bpm: {bpm}")
        self.ms_per_beat = int((60 * 1000) / bpm)

    def update(self) -> bool:
        now = time.monotonic_ns()
        diff = now - self.last
        self.acc += diff
        self.last = now

        ns = self.ms_per_beat * 1000 * 1000
        if self.acc >= ns:
            self.acc -= ns
            return True

        return False


class Tempo:
    def __init__(self, on_tick):
        self.internal_multiplier = 4
        self.on_tick = on_tick
        self.use_internal = True

        self.internal_tempo = InternalTempo(120 * self.internal_multiplier)
        self.midi_tempo = MidiTempo()

    def set_bpm(self, bpm):
        self.internal_tempo.set_bpm(bpm * self.internal_multiplier)

    def update(self):
        if self.use_internal:
            if self.internal_tempo.update():
                self.on_tick()

    def on_midi_msg(self, msg):
        if not self.use_internal:
            self.midi_tempo.handle_message(msg, self.on_tick)
