from firmware.drum import Drum  # type: ignore
from .hardware import (
    Display,
    Drumpad,
    SequencerKey,
    saturated_multiply,
    # SampleSelectKey,
    # Direction,
    ControlKey,
    ControlName,
)


def hex_to_rgb(hex_color):
    # Convert the hexadecimal values to decimal and create a tuple of (R, G, B)
    rgb = tuple(int(hex_color[i : i + 2], 16) for i in (0, 2, 4))

    return rgb


def int_to_rgb(int_color):
    # Extract the red component (bits 16-23) using bitwise AND and right shift
    r = (int_color >> 16) & 0xFF

    # Extract the green component (bits 8-15) using bitwise AND and right shift
    g = (int_color >> 8) & 0xFF

    # Extract the blue component (bits 0-7) using bitwise AND
    b = int_color & 0xFF

    return (r, g, b)


FADE_TIME_MS = 150


class PadIndicator:
    def __init__(self, index):
        self.index = index
        self.fade_remaining_ms = 0
        self.last_triggered_step = 0

    def show(self, display, step_index, color, active):
        fade_amount = 0

        if active:
            if self.fade_remaining_ms > 0:
                fade_amount = self.fade_remaining_ms / FADE_TIME_MS

            if active and step_index != self.last_triggered_step:
                self.fade_remaining_ms = FADE_TIME_MS

        self.last_triggered_step = step_index
        pad = Drumpad(self.index)
        display.set_color(pad, saturated_multiply(color, 1 - fade_amount))

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
            key = SequencerKey(step_index, self.track_index)
            if track.repeat_is_active():
                display.set_color(key, step_color)
            elif self.fade_remaining_ms > 0:
                fade_amount = self.fade_remaining_ms / FADE_TIME_MS
                display.set_color(key, saturated_multiply(step_color, fade_amount))
            elif step.active:
                display.set_color(key, step_color)

    def update(self, delta_ms: int) -> None:
        if self.fade_remaining_ms > 0:
            self.fade_remaining_ms -= delta_ms


class Cursor:
    def __init__(self):
        self.fade_play_toggle = False
        self.last_beat_position = 0

    def show(
        self,
        display: Display,
        color: int,
        playing: bool,
        track_index: int,
        current_step: int,
        beat_position: float,
    ) -> None:
        amount = min(max(0, 1 - beat_position), 1)
        sequencer_key = SequencerKey(current_step, track_index)
        start_key = ControlKey(ControlName.Start)

        if beat_position < self.last_beat_position:
            self.fade_play_toggle = not self.fade_play_toggle

        self.last_beat_position = beat_position
        if playing:
            display.blend(sequencer_key, color, (amount * 0.7) + 0.3)
            display.set_color(start_key, color)
        else:
            if self.fade_play_toggle:
                display.blend(sequencer_key, color, (amount * 0.7) + 0.3)
                display.set_color(start_key, (0, 0, 0))
            else:
                display.set_color(start_key, color)
                display.fade(start_key, amount)


class PizzaView:
    def __init__(self, track_count, config) -> None:
        self.pad_indicators = [
            PadIndicator(track_index) for track_index in range(track_count)
        ]
        self.config = config
        self.rings = [SequencerRing(track_index) for track_index in range(track_count)]
        self.cursor = Cursor()

    def update(self, delta_ms: int) -> None:
        for pad_indicator in self.pad_indicators:
            pad_indicator.update(delta_ms)

        for ring in self.rings:
            ring.update(delta_ms)

    def trigger_track(self, track_index: int) -> None:
        self.rings[track_index].trigger()

    def show(self, display: Display, drum: Drum, beat_position: float) -> None:
        for track_index, track in enumerate(drum.tracks):
            current_step = drum.get_indicator_step(track_index)
            # TODO: only retrieve colors once, or at least cache them
            color = int_to_rgb(int(drum.config.get(f"note.{track.note}.color")))
            self.rings[track_index].show_steps(display, drum, color)
            self._show_pad(display, drum, color, track_index)

            self.cursor.show(
                display,
                int_to_rgb(int(self.config.get("device.cursor_color"))),
                drum.playing,
                track_index,
                current_step,
                beat_position,
            )

    def _show_pad(self, display, drum, color, track_index) -> None:
        current_step_index = drum.get_indicator_step(track_index)

        track = drum.tracks[track_index]
        step_active = track.sequencer.steps[current_step_index].active
        self.pad_indicators[track_index].show(
            display, current_step_index, color, step_active
        )
