import time

TEMPO_TICKS_PER_BEAT = 24
TICK_SUBDIVISIONS = 6
TICKS_PER_BEAT = TEMPO_TICKS_PER_BEAT * TICK_SUBDIVISIONS


class TempoSource:
    Internal = 1
    MIDI = 2
    # Sync = 3


class Divider:
    def __init__(self, division) -> None:
        self.division = division
        self.counter = 0

    def tick(self) -> bool:
        triggered = False
        if self.counter % self.division == 0:
            self.counter = 0
            triggered = True

        self.counter += 1
        return triggered

    def reset(self) -> None:
        self.counter = 0

    def set_division(self, division) -> None:
        self.division = division


class InternalTicker:
    def __init__(self, bpm=100) -> None:
        self.accumulated_ns = 0
        self.last_ns = time.monotonic_ns()
        self.set_bpm(bpm)

    def set_bpm(self, bpm) -> None:
        self._ns_per_tick = int((1_000_000 * 60_000) / (bpm * TICKS_PER_BEAT))

    def update(self, on_tick) -> None:
        now = time.monotonic_ns()
        diff = now - self.last_ns
        self.accumulated_ns += diff
        self.last_ns = now

        while self.accumulated_ns >= self._ns_per_tick:
            self.accumulated_ns -= self._ns_per_tick
            on_tick()


class Swing:
    Range = 6  # Delay in tempo ticks in each direction

    def __init__(self) -> None:
        self._amount = 0
        self.reset_ticks()

    def reset_ticks(self) -> None:
        self._ticks = 0
        self._even_beat = True

    def set_amount(self, amount) -> None:
        self._amount = max(-Swing.Range, min(Swing.Range, amount))

    def adjust(self, direction) -> None:
        addition = 1 if direction > 0 else -1
        self.set_amount(self._amount + addition)

    def tick(self, tick_count, on_half_beat) -> None:
        triggered = False
        mid_point = self._get_middle_tick()

        if self._even_beat and self._ticks % TICKS_PER_BEAT == 0:
            self._ticks = 0
            triggered = True

        elif not self._even_beat and self._ticks % mid_point == 0:
            triggered = True

        if triggered:
            self._even_beat = not self._even_beat
            on_half_beat()

        self._ticks = (self._ticks + tick_count) % TICKS_PER_BEAT

    def is_tempo_tick(self):
        return self._ticks % TICK_SUBDIVISIONS == 0

    def _get_middle_tick(self) -> int:
        return int(TICKS_PER_BEAT / 2) + (self._amount * TICK_SUBDIVISIONS)

    def get_beat_position(self) -> float:
        middle_tick = self._get_middle_tick()
        if self._ticks < middle_tick:
            return self._ticks / middle_tick
        else:
            return (self._ticks - middle_tick) / (TICKS_PER_BEAT - middle_tick)


class Tempo:
    def __init__(self, tempo_tick_callback, half_beat_callback) -> None:
        self.tempo_tick_callback = tempo_tick_callback
        self.half_beat_callback = half_beat_callback
        self.tempo_source = TempoSource.Internal
        self.internal_ticker = InternalTicker()
        self.swing = Swing()

    def set_bpm(self, bpm) -> None:
        self.internal_ticker.set_bpm(bpm)

    def reset(self) -> None:
        self.swing.reset_ticks()

    def update(self) -> None:
        if TempoSource.Internal == self.tempo_source:
            self.internal_ticker.update(self._on_internal_tick)

    def handle_midi_clock(self) -> None:
        if TempoSource.MIDI == self.tempo_source:
            self.tempo_tick_callback(self.tempo_source)
            self.swing.tick(TICK_SUBDIVISIONS, self.half_beat_callback)

    def _on_internal_tick(self):
        self.swing.tick(1, self.half_beat_callback)
        if self.swing.is_tempo_tick():
            self.tempo_tick_callback(self.tempo_source)

    def get_beat_position(self) -> float:
        return self.swing.get_beat_position()
