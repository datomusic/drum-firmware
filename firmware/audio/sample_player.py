import board
import audiopwmio
import audiocore
import audiomixer


class SamplePlayer:
    def __init__(self):
        self.audio = audiopwmio.PWMAudioOut(board.D12)
        self.mixer = audiomixer.Mixer(
            voice_count=4, sample_rate=16000, buffer_size=128, channel_count=1
        )

        self.audio.play(self.mixer)

        sample_names = [
            "closed_hh.wav",
            "sample.wav",
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

    def play(self, channel, note):
        index = channel % self.sample_count
        sample = self.samples[index]
        voice = self.mixer.voice[index]

        voice.play(sample)
