from brains2.brains2 import ControlName, SequencerKey, KeyboardKey, ControlKey, Controls, Display
from drum import Drum


class DrumControls:
    def __init__(self):
        self.current_track = 0
        self.controls = Controls()
        self.display = Display()

    def show(self, drum):
        self.display.clear()

        show_track(self.display,
                   Colors.Tracks[self.current_track],
                   drum.tracks[self.current_track])

        self.display.show()

    def handle_input(self, drum: Drum):
        event = self.controls.get_key_event()
        if event:
            key = event.key

            if event.pressed:
                color = (255, 0, 0)
                self.display.set_color(key, color)

            if isinstance(key, SequencerKey):
                print(f"Seq, step: {key.step}, pressed: {event.pressed}")
                if event.pressed:
                    drum.tracks[self.current_track].sequencer.toggle_step(
                        key.step)

            elif isinstance(key, KeyboardKey):
                track = drum.tracks[self.current_track]
                track.note = key.number
                print(f"track.note: {track.note}")
                print(
                    f"Keyboard, number: {key.number}, pressed: {event.pressed}"
                )

            elif isinstance(key, ControlKey):
                print(f"Control, name: {key.name}, pressed: {event.pressed}")
                self.current_track = track_from_key(key)


def track_from_key(key: ControlKey) -> int:
    if key.name == ControlName.Seq1:
        return 0
    elif key.name == ControlName.Seq2:
        return 1
    elif key.name == ControlName.Down:
        return 2
    elif key.name == ControlName.Up:
        return 3
    else:
        return 0


class Colors:
    CurStep = (50, 50, 50)
    Tracks = (
        (200, 0, 0),
        (0, 200, 200),
        (0, 0, 200),
        (200, 0, 200)
    )


def show_track(display, step_color, track):
    for (step_ind, step) in enumerate(track.sequencer.steps):
        col = None
        if step_ind == track.sequencer.cur_step_index:
            col = Colors.CurStep
        elif step.active:
            col = step_color

        display.set_color(SequencerKey(step_ind), col)

    for n in range(10):
        col = None
        if n == track.note:
            col = step_color
        display.set_color(KeyboardKey(n), col)
