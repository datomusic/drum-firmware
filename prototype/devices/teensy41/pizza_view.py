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


def fade_color(color, amount):
    (r, g, b) = color
    return (int(r * amount), int(g * amount), int(b * amount))


class PadIndicator:
    FadeTimeMs = 150

    def __init__(self, index):
        self.index = index
        self.fade_remaining_ms = 0
        self.last_triggered_step = 0

    def trigger(self, display, step_index, color, active):
        if active:
            if self.fade_remaining_ms > 0:
                fade_amount = self.fade_remaining_ms / PadIndicator.FadeTimeMs
                color = fade_color(color, 1 - fade_amount)

            if active and step_index != self.last_triggered_step:
                self.fade_remaining_ms = PadIndicator.FadeTimeMs

        self.last_triggered_step = step_index
        pad = Drumpad(self.index)
        display.set_color(pad, color)

    def update(self, delta_ms: int):
        if self.fade_remaining_ms > 0:
            self.fade_remaining_ms -= delta_ms


class PizzaView():
    def __init__(self):
        self.pad_indicators = [PadIndicator(
            track_index) for track_index in range(Track.Count)]

    def update(self, delta_ms: int) -> None:
        for pad_indicator in self.pad_indicators:
            pad_indicator.update(delta_ms)

    def render(self, display: Display, drum: Drum, drum_trigger_states) -> None:
        for (track_index, _track) in enumerate(drum.tracks):
            color = ColorScheme.Tracks[drum.tracks[track_index].note]
            show_track(display, drum, color, track_index,
                       drum_trigger_states[track_index])
            self._show_pads(display, drum, color, track_index)

    def _show_pads(self, display, drum, color, track_index) -> None:
        current_step_index = drum.get_indicator_step(track_index)

        track = drum.tracks[track_index]
        step_active = track.sequencer.steps[current_step_index].active
        self.pad_indicators[track_index].trigger(
            display, current_step_index, color, step_active)


def show_track(display, drum: Drum, step_color, track_index: int, trigger_down) -> None:
    track = drum.tracks[track_index]
    current_step_index = drum.get_indicator_step(track_index)

    for step_index, step in enumerate(track.sequencer.steps):
        on_cursor = step_index == current_step_index
        color = None
        if on_cursor:
            color = ColorScheme.Cursor
        elif step.active or trigger_down:
            color = step_color

        display.set_color(SequencerKey(step_index, track_index), color)
