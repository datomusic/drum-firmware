from firmware.drum import Drum, Track  # type: ignore
from .colors import ColorScheme
from .hardware import (
    Display,
    Drumpad,
    SequencerKey,
    # SampleSelectKey,
    # Direction,
    # ControlKey,
    # ControlName
)


FADE_TIME_MS = 150


def fade_color(color, amount):
    (r, g, b) = color
    return (int(r * amount), int(g * amount), int(b * amount))


class PadIndicator:
    def __init__(self, index):
        self.index = index
        self.fade_remaining_ms = 0
        self.last_triggered_step = 0

    def show(self, display, step_index, color, active):
        if active:
            if self.fade_remaining_ms > 0:
                fade_amount = self.fade_remaining_ms / FADE_TIME_MS
                color = fade_color(color, 1 - fade_amount)

            if active and step_index != self.last_triggered_step:
                self.fade_remaining_ms = FADE_TIME_MS

        self.last_triggered_step = step_index
        pad = Drumpad(self.index)
        display.set_color(pad, color)

    def update(self, delta_ms: int):
        if self.fade_remaining_ms > 0:
            self.fade_remaining_ms -= delta_ms


class SequencerRing:
    def __init__(self, track_index: int) -> None:
        self.track_index = track_index
        self.fade_remaining_ms = 0

    def trigger(self):
        self.fade_remaining_ms = FADE_TIME_MS

    def show_steps(self, display: Display, drum: Drum, step_color) -> None:
        track = drum.tracks[self.track_index]

        for step_index, step in enumerate(track.sequencer.steps):
            color = None
            if self.fade_remaining_ms > 0:
                fade_amount = self.fade_remaining_ms / FADE_TIME_MS
                color = fade_color(step_color, fade_amount)
            elif step.active:
                color = step_color

            display.set_color(
                SequencerKey(step_index, self.track_index),
                color)

    def update(self, delta_ms: int) -> None:
        if self.fade_remaining_ms > 0:
            self.fade_remaining_ms -= delta_ms


class Cursor:
    def update(self, delta_ms: int):
        pass

    def show(self, display: Display,  drum: Drum):
        for track_index in range(Track.Count):
            step_index = drum.get_indicator_step(track_index)
            display.set_color(
                SequencerKey(step_index, track_index),
                ColorScheme.Cursor
            )


class PizzaView():
    def __init__(self) -> None:
        self.cursor = Cursor()
        self.pad_indicators = [
            PadIndicator(track_index)
            for track_index in range(Track.Count)
        ]

        self.rings = [
            SequencerRing(track_index)
            for track_index in range(Track.Count)
        ]

    def update(self, delta_ms: int) -> None:
        self.cursor.update(delta_ms)

        for pad_indicator in self.pad_indicators:
            pad_indicator.update(delta_ms)

        for ring in self.rings:
            ring.update(delta_ms)

    def trigger_track(self, track_index: int) -> None:
        self.rings[track_index].trigger()

    def show(self, display: Display, drum: Drum) -> None:
        for (track_index, track) in enumerate(drum.tracks):
            color = ColorScheme.Tracks[drum.tracks[track_index].note]
            self.rings[track_index].show_steps(display, drum, color)
            self._show_pad(display, drum, color, track_index)
            self.cursor.show(display, drum)

    def _show_pad(self, display, drum, color, track_index) -> None:
        current_step_index = drum.get_indicator_step(track_index)

        track = drum.tracks[track_index]
        step_active = track.sequencer.steps[current_step_index].active
        self.pad_indicators[track_index].show(
            display, current_step_index, color, step_active)
