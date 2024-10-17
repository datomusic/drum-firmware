from ..output_api import Output, OutputChannelParam
from .sample_player import SamplePlayer
import audiopwmio  # type: ignore
import board  # type: ignore


class AudioOutput(Output):
    def __init__(self) -> None:
        # A reference to the audio object must be stored,
        # otherwise it is garbage collected and the audio stops.
        self.audio = audiopwmio.PWMAudioOut(board.D12)
        self.player = SamplePlayer(self.audio)

    def send_note_on(self, channel: int, _note: int, _vel_percent: float):
        self.player.play_sample(channel)

    def send_note_off(self, channel: int, note: int):
        pass

    def set_channel_param(self, channel: int, param, value_percent: float):
        if param == OutputChannelParam.Pitch:
            self.player.set_pitch(channel, value_percent / 50)
        elif param == OutputChannelParam.Mute:
            pass
        else:
            raise TypeError(f"Invalid output channel parameter: {param}")

    def on_tempo_tick(self, source) -> None:
        pass

    def set_param(self, param, value_percent: float):
        pass
