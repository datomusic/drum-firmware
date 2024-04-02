from .hardware import (
    ControlName,
    SequencerKey,
    KeyboardKey,
    ControlKey,
    Controls,
    Display,
)

from drum import Drum
from note_output import NoteOutput
from device_api import DeviceAPI


class OverkillDevice(DeviceAPI):
    def __init__(self):
        self.current_track = 0
        self.controls = Controls()
        self.display = Display()

    def show(self, drum):
        self.display.clear()

        for track_index in range(0, 4):
            show_track(
                self.display,
                ColorScheme.Tracks[track_index],
                drum.tracks[track_index],
                track_index,
            )

        self.display.show()

    def handle_input(self, drum: Drum, note_out: NoteOutput):
        event = self.controls.get_key_event()
        if event and event.pressed:
            key = event.key

            if isinstance(key, SequencerKey):
                print(f"Seq, step: {key.step}, pressed: {event.pressed}")
                drum.tracks[key.track].sequencer.toggle_step(key.step)

            elif isinstance(key, KeyboardKey):
                track = drum.tracks[self.current_track]
                track.note = key.number
                print(f"track.note: {track.note}")
                print(f"Keyboard, number: {key.number}, pressed: {event.pressed}")

            elif isinstance(key, ControlKey):
                print(f"Control, name: {key.name}, pressed: {event.pressed}")
                if key.name == ControlName.Seq1:
                    self.current_track = 0
                elif key.name == ControlName.Seq2:
                    self.current_track = 1
                elif key.name == ControlName.Down:
                    self.current_track = 2
                elif key.name == ControlName.Up:
                    self.current_track = 3
                elif key.name == ControlName.Start:
                    note_out.play(drum.tracks[self.current_track].note)

            else:
                print(event)


class ColorScheme:
    Cursor = (50, 50, 50)
    Tracks = ((200, 200, 0), (0, 200, 0), (0, 0, 200), (200, 0, 0))


def show_track(display, step_color, track, track_index):
    for step_index, step in enumerate(track.sequencer.steps):
        color = None
        if step_index == (track.sequencer.cur_step_index + 7) % 8:
            color = ColorScheme.Cursor
        elif step.active:
            color = step_color

        display.set_color(SequencerKey(step_index, track_index), color)
