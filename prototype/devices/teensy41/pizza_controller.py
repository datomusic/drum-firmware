from firmware.device_api import Controller, Controls, SampleChange
from firmware.drum import Drum

from .hardware import (
    Teensy41Hardware,
    Display,
    Drumpad,
    SequencerKey,
    SampleSelectKey,
    Direction
)

from .reading import IncDecReader, PotReader, bpm_from_pot
from .colors import ColorScheme


# TODO:
# - Move inversion to hardware layer
# - Maybe move jitter prevention to hardware layer

class PizzaController(Controller):
    def __init__(self):
        self.display = Display()

        self.hardware = Teensy41Hardware()

        self.speed_setting = PotReader(self.hardware.speed_pot)
        self.filter_setting = IncDecReader(
            self.hardware.filter_left, self.hardware.filter_right)

    def update(self, controls: Controls) -> None:
        self._read_pots(controls)
        self._process_keys(controls)

    def show(self, drum: Drum):
        self.display.clear()

        for track_index in range(0, 4):
            self.display.set_color(
                Drumpad(track_index),
                ColorScheme.Tracks[drum.tracks[track_index].note]
            )
            show_track(
                self.display,
                ColorScheme.Tracks[drum.tracks[track_index].note],
                drum.tracks[track_index],
                track_index,
            )

        self.display.show()

    def _read_pots(self, controls: Controls) -> None:
        self.speed_setting.read(
            lambda speed: controls.set_bpm(bpm_from_pot(speed)))

        self.filter_setting.read(
            lambda val: controls.adjust_filter(val))

    def _process_keys(self, controls: Controls) -> None:
        event = self.hardware.get_key_event()
        pressed = event and event.pressed
        if pressed:
            key = event.key
            if isinstance(key, SequencerKey):
                controls.toggle_track_step(key.track, key.step)

            elif isinstance(key, SampleSelectKey):
                if key.direction == Direction.Down:
                    change = SampleChange.Prev
                elif key.direction == Direction.Up:
                    change = SampleChange.Next

                controls.change_sample(key.track, change)

            # elif isinstance(key, ControlKey):
            #     print(f"Control, name: {key.name}, pressed: {event.pressed}")
            #     if key.name == ControlName.Start:
            #         note_out.play(drum.tracks[self.current_track].note)

            #     print(f"key: {key}")


def show_track(display, step_color, track, track_index):
    for step_index, step in enumerate(track.sequencer.steps):
        color = None
        if step_index == (track.sequencer.cur_step_index + 7) % 8:
            color = ColorScheme.Cursor
        elif step.active:
            color = step_color

        display.set_color(SequencerKey(step_index, track_index), color)
