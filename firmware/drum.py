from .tempo import Tempo
from .output_api import Output
from .sequencer import Sequencer
from .settings import Settings


class Drum:
    def __init__(self, track_count: int, output: Output, settings: Settings):
        self.output = output
        self.sequencer = Sequencer(
            output, track_count, settings, self._on_sequencer_play)

        self.tempo = Tempo(
            tempo_tick_callback=self.output.on_tempo_tick,
            on_quarter_beat=self.sequencer.on_quarter_beat,
        )

    def _on_sequencer_play(self):
        self.tempo.reset()
