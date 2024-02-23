from sequencer import Sequencer
from tempo import Tempo


class Drum:
    def __init__(self):
        self.seqs = [Sequencer() for i in range(4)]
        self.tempo = Tempo()

    def run(self):
        pass
