from teensy41.teensy41 import ControlName, SequencerKey, SampleSelectKey, ControlKey, Controls, Display, Drumpad
from drum import Drum
from note_output import NoteOutput


class DrumControls:
    def __init__(self):
        self.current_track = 0
        self.controls = Controls()
        self.display = Display()

    def show(self, drum):
        self.display.clear()

        for track_index in range(0, 4):
            self.display.set_color(Drumpad(track_index), ColorScheme.Tracks[drum.tracks[track_index].note])
            show_track(self.display,
                   ColorScheme.Tracks[drum.tracks[track_index].note],
                   drum.tracks[track_index],track_index)

        self.display.show()

    def handle_input(self, drum: Drum, note_out: NoteOutput):
        event = self.controls.get_key_event()
        if event and event.pressed:
            key = event.key

            if isinstance(key, SequencerKey):
                print(f"Track: {key.track}, step: {key.step}, pressed: {event.pressed}")
                if event.pressed:
                    drum.tracks[key.track].sequencer.toggle_step(key.step)

            elif isinstance(key, SampleSelectKey):
                track = drum.tracks[key.track]
                # track.note = key.number
                print(f"track.note: {track.note}")
                print(
                    f"Sample Select, number: {key.track}, pressed: {event.pressed}"
                )
                track.note = track.note + key.direction
                if (track.note < 0):
                    track.note = 31
                if(track.note > 31):
                    track.note = 0

            elif isinstance(key, ControlKey):
                print(f"Control, name: {key.name}, pressed: {event.pressed}")
                if key.name == ControlName.Start:
                    note_out.play(drum.tracks[self.current_track].note)
            
            else:
                print(event)

def show_track(display, step_color, track, track_index):
    for (step_index, step) in enumerate(track.sequencer.steps):
        color = None
        if step_index == (track.sequencer.cur_step_index + 7) % 8:
            color = ColorScheme.Cursor
        elif step.active:
            color = step_color

        display.set_color(SequencerKey(step_index,track_index), color)