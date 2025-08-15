from .tempo import Tempo
from .output_api import Output
from .sequencer import Sequencer, STEP_COUNT
from .settings import Settings

TRACK_COUNT = 4


class Drum:
    def __init__(self, output: Output, settings: Settings):
        self.output = output
        self.sequencer = Sequencer(
            output, TRACK_COUNT, settings, self._on_sequencer_play)

        self.tempo = Tempo(
            tempo_tick_callback=self.output.on_tempo_tick,
            on_quarter_beat=self.sequencer.on_quarter_beat,
        )

        _setup_tracks(self.sequencer.tracks, settings)

    def _on_sequencer_play(self):
        self.tempo.reset()


def _setup_tracks(tracks, settings: Settings):
    for i in range(TRACK_COUNT):
        tracks[i].note = settings.get(f"track.{i}.init_note")
        track_init = int(settings.get(f"track.{i}.init_pattern"))
        for j in range(STEP_COUNT):
            if track_init & (1 << j):
                tracks[i].steps.set_step(8 - j - 1, 100)
