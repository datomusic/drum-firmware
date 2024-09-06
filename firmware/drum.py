from .tempo import Tempo
from .output_api import Output
from .sequencer import Sequencer


class Drum:
    def __init__(self, sequencer: Sequencer, output: Output, tempo: Tempo):
        self.tempo = tempo
        self.sequencer = sequencer
        self.output = output

    def set_playing(self, playing):
        self.sequencer.playing = playing
        if playing:
            self.tempo.reset()
