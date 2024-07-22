from .device_api import Output

import board
import audiocore
import audiopwmio


class AudioOutput(Output):
    def __init__(self):
        data = open("sample.wav", "rb")
        self.sample = audiocore.WaveFile(data)
        self.audio = audiopwmio.PWMAudioOut(board.D12)

    def send_note_on(self, channel: int, note: int, vel_percent: float):
        self.audio.play(self.sample)

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
