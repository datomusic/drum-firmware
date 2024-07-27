from .device_api import Output

import board
import audiocore
import audiopwmio


class AudioOutput(Output):
    def __init__(self):
        self.audio = audiopwmio.PWMAudioOut(board.D12)
        sample_names = [
            "closed_hh.wav",
            "kick.wav",
            "open_hh.wav",
            "snare.wav",
        ]

        self.samples = list(map(
            lambda file_name: audiocore.WaveFile(open(f"samples/{file_name}", "rb")),
            sample_names,
        ))

        self.sample_count = len(self.samples)

    def send_note_on(self, channel: int, note: int, vel_percent: float):
        self.audio.play(self.samples[channel % self.sample_count])

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
