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


class PizzaView():
    def __init__(self):
        pass

    def render(self, display: Display, drum: Drum) -> None:
        for track_index in range(0, 4):
            display.set_color(
                Drumpad(track_index),
                ColorScheme.Tracks[drum.tracks[track_index].note]
            )

            show_track(
                display,
                ColorScheme.Tracks[drum.tracks[track_index].note],
                drum.get_indicator_step(),
                drum.tracks[track_index],
                track_index,
            )


def show_track(display, step_color, cur_step_index, track: Track, track_index
               ) -> None:
    for step_index, step in enumerate(track.sequencer.steps):
        color = None
        if step_index == (cur_step_index + 7) % 8:
            color = ColorScheme.Cursor
        elif step.active:
            color = step_color

        display.set_color(SequencerKey(step_index, track_index), color)
