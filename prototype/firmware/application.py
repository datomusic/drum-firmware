from .note_player import NotePlayer
from .drum import Drum
from .device_api import Controller, Output

USE_INTERNAL_TEMPO = True


def setup_tracks(tracks):
    tracks[0].note = 0
    tracks[1].note = 7
    tracks[2].note = 15
    tracks[3].note = 23

    tracks[0].sequencer.set_step(0)
    tracks[0].sequencer.set_step(4)
    tracks[1].sequencer.set_step(3)
    tracks[1].sequencer.set_step(5)
    tracks[2].sequencer.set_step(7)
    tracks[3].sequencer.set_step(6)


class Application:
    def __init__(self, controllers: list[Controller], output: Output):
        self.controllers = controllers
        self.output = output
        note_out = NotePlayer(output.send_note_on, output.send_note_off)
        self.drum = Drum(note_out)

        setup_tracks(self.drum.tracks)
        self.drum.tempo.use_internal = USE_INTERNAL_TEMPO

    def update(self) -> None:
        for controller in self.controllers:
            controller.update(self.drum, self.output)

        self.drum.update()

    def show(self) -> None:
        for controller in self.controllers:
            controller.show(self.drum)

    def run(self):
        while True:
            self.update()
            self.show()
