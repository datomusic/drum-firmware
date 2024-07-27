from .device_api import Output

import board
import audiocore
import audiopwmio
import audiomixer


class AudioOutput(Output):
    def __init__(self):
        self.audio = audiopwmio.PWMAudioOut(board.D12)
        self.mixer = audiomixer.Mixer(
            voice_count=4, sample_rate=16000, buffer_size=128, channel_count=1
        )
        self.audio.play(self.mixer)

        sample_names = [
            "closed_hh.wav",
            "006.wav",
            "open_hh.wav",
            "snare.wav",
        ]

        self.samples = list(
            map(
                lambda file_name: audiocore.WaveFile(
                    open(f"samples/{file_name}", "rb")
                ),
                sample_names,
            )
        )

        self.sample_count = len(self.samples)

        for voice in self.mixer.voice:
            voice.level = 0.8

    def send_note_on(self, channel: int, note: int, vel_percent: float):
        index = channel % self.sample_count
        sample = self.samples[index]
        voice = self.mixer.voice[index]

        voice.play(sample)

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
