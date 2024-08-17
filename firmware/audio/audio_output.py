from ..device_api import Output
from .sample_player import SamplePlayer


class AudioOutput(Output):
    def __init__(self):
        self.player = SamplePlayer()

    def send_note_on(self, channel: int, note: int, vel_percent: float):
        print("Audio note_on")
        self.player.play(channel, note)

    def send_note_off(self, channel: int, note: int):
        pass

    def set_channel_pitch(self, channel: int, pitch_percent: float):
        pass

    def set_channel_mute(self, channel: int, pressure: float):
        pass

    def set_param(self, param, value) -> None:
        pass

    def on_tempo_tick(self, source) -> None:
        pass
