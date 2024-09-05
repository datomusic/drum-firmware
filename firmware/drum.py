from .tempo import Tempo
from .device_api import Output
from .sequencer import Sequencer


class Drum:
    def __init__(self, sequencer: Sequencer, output: Output, tempo: Tempo):
        self.tempo = tempo
        self.sequencer = sequencer
        self.output = output
