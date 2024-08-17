import board
import audiocore
import audiomixer
import adafruit_wave
import synthio
from .sample import Sample


class SamplePlayer:
    def __init__(self, audio):
        self.mixer = audiomixer.Mixer(
            voice_count=4, sample_rate=16000, buffer_size=128, channel_count=1
        )
        self.synth = synthio.Synthesizer(channel_count=1, sample_rate=44100)
        audio.play(self.mixer)
        self.mixer.voice[0].play(self.synth)
        self.mixer.voice[0].level = 0.75


        sample_names = [
            "closed_hh.wav",
            "sample.wav",
            "open_hh.wav",
            "snare.wav",
        ]

        self.samples = list(map(Sample, sample_names))
        self.sample_count = len(self.samples)

        for voice in self.mixer.voice:
            voice.level = 0.8

    def play_sample(self, sample_index: int):
        index = sample_index % self.sample_count
        sample = self.samples[index]
        sample.press(self.synth, 64)
        sample.release(self.synth, 64)


