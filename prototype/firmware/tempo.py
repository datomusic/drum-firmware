from adafruit_midi.timing_clock import TimingClock  # type: ignore
from adafruit_midi.midi_continue import Continue  # type: ignore

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
        self.acc_ns = 0
        self.last = time.monotonic_ns()
        self.set_bpm(bpm)
        self.swing_multiplier = 0
        self.even_step = True

    def set_bpm(self, bpm):
        self.bpm = int(max(1, bpm))

    def adjust_swing(self, amount_percent):
        new_swing = self.swing_multiplier + amount_percent / 100
        self.swing_multiplier = max(-1, min(1, new_swing))

    def reset_swing(self):
        self.swing_multiplier = 0

    def update(self) -> bool:
        now = time.monotonic_ns()
        diff = now - self.last
        self.acc_ns += diff
        self.last = now

        ns = self._next_beat_ms() * 1000 * 1000
        if self.acc_ns >= ns:
            self.acc_ns -= ns
            self.even_step = not self.even_step
            return True

        return False

    def _next_beat_ms(self):
        ms_per_beat = int((60 * 1000) / self.bpm)
        direction = 0

        if self.swing_multiplier > 0.05:
            if self.even_step:
                direction = 1
            else:
                direction = -1

        elif self.swing_multiplier < -0.05:
            if self.even_step:
                direction = -1
            else:
                direction = 1

        return int(ms_per_beat * (1 + self.swing_multiplier * direction))


class Tempo:
    def __init__(self, on_tick):
        self.internal_multiplier = 2
        self.on_tick = on_tick
        self.use_internal = True

        self.internal_tempo = InternalTempo(120 * self.internal_multiplier)
        self.midi_tempo = MidiTempo()

    def set_bpm(self, bpm):
        bpm = round(bpm)
        print(f"bpm: {bpm}")
        self.internal_tempo.set_bpm(bpm * self.internal_multiplier)

    def update(self):
        if self.use_internal:
            while self.internal_tempo.update():
                self.on_tick()

    def on_midi_msg(self, msg):
        if not self.use_internal:
            self.midi_tempo.handle_message(msg, self.on_tick)
