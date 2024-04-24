from adafruit_midi.timing_clock import TimingClock  # type: ignore
from adafruit_midi.midi_continue import Continue  # type: ignore

import time

TICKS_PER_BEAT = 24


class TempoSource:
    Internal = 1
    # MIDI = 2
    # Sync = 3


class BeatTicks:
    def __init__(self, ticks_per_beat):
        self.ticks_per_beat = ticks_per_beat
        self.ticks = 0

    def tick(self):
        ret = False
        if self.ticks % self.ticks_per_beat == 0:
            self.ticks = 0
            ret = True

        self.ticks += 1
        return ret

    def reset(self):
        self.ticks = 0


class MidiTempo:
    def __init__(self):
        self.ticks = BeatTicks(TICKS_PER_BEAT / 2)

    def handle_message(self, msg, on_tick):
        if type(msg) is TimingClock:
            self.ticks.tick(on_tick)

        elif type(msg) is Continue:
            self.ticks.reset()
            on_tick()


class InternalTempo:
    def __init__(self, bpm=100):

        self.acc_ns = 0
        self.last = time.monotonic_ns()
        self.set_bpm(bpm)
        self.swing_multiplier = 0
        self.even_step = True
        self.half_note_ticks = BeatTicks(TICKS_PER_BEAT / 2)

    def set_bpm(self, bpm):
        self.bpm = int(max(1, bpm))

    def adjust_swing(self, amount_percent):
        new_swing = self.swing_multiplier + amount_percent / 100
        self.swing_multiplier = max(-1, min(1, new_swing))

    def reset_swing(self):
        self.swing_multiplier = 0

    def update(self, on_tick, on_half_beat) -> None:
        now = time.monotonic_ns()
        diff = now - self.last
        self.acc_ns += diff
        self.last = now
        
        ns = self._next_tick_ns()
        while self.acc_ns >= ns:
            ns = self._next_tick_ns()
            self.acc_ns -= ns
            on_tick()
            if self.half_note_ticks.tick():
                self.even_step = not self.even_step
                on_half_beat()

    def _next_tick_ns(self):
        ms_per_tick = int((60 * 1000) / (self.bpm * TICKS_PER_BEAT))
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

        ms = int(ms_per_tick * (1 + self.swing_multiplier * direction))
        return ms * 1000 * 1000

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
    def __init__(self, on_tick, on_half_beat):
        self.on_tick = on_tick
        self.on_half_beat = on_half_beat
        self.tempo_source = TempoSource.Internal

        self.internal_tempo = InternalTempo()
        self.midi_tempo = MidiTempo()

    def set_bpm(self, bpm):
        self.internal_tempo.set_bpm(bpm)

    def update(self):
        def on_tick():
            self.on_tick(self.tempo_source)

        if TempoSource.Internal == self.tempo_source:
            self.internal_tempo.update(on_tick, self.on_half_beat)

    def on_midi_msg(self, msg):
        if not self.use_internal:
            self.midi_tempo.handle_message(msg, self.on_tick)
