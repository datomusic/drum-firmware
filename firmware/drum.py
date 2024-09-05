from .tempo import Tempo
from .device_api import Output
from .sequencer import Sequencer


class Drum:
    def __init__(self, sequencer: Sequencer, output: Output, tempo: Tempo, on_sample_trigger):
        self.tempo = tempo
        self.sequencer = sequencer
        self.output = output
        self.on_sample_trigger = on_sample_trigger

    def play_track_sample(self, track_index: int, velocity: float):
        self.sequencer.tracks[track_index].play(velocity)
        self.on_sample_trigger(track_index)

    def set_playing(self, playing):
        self.sequencer.playing = playing
        if playing:
            self.tempo.reset()
