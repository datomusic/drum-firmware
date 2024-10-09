import adafruit_wave  # type: ignore
import ulab.numpy as np  # type: ignore
import audiomixer  # type: ignore
import audiocore  # type: ignore


class MonoSample:
    BUFFER_SIZE = 256

    def __init__(self, path):
        file = adafruit_wave.open(path)
        channels, bits_per_sample, sample_rate = (
            file.getnchannels(),
            file.getsampwidth() * 8,
            file.getframerate(),
        )
        self.bits_per_sample = bits_per_sample
        self.rate = sample_rate

        assert channels == 1

        self.buffer_data_type = np.int16 if bits_per_sample == 16 else np.uint8

        self.frame_count = file.getnframes()
        self.file_buffer = np.frombuffer(
            file.readframes(MonoSample.BUFFER_SIZE),
            dtype=self.buffer_data_type
        )
        self.file_buffer_length = len(self.file_buffer)
        self.file_buffer_indices = np.arange(self.file_buffer_length)

    def is_signed(self):
        return self.bits_per_sample == 16

    def play_at_speed(self, player, speed_multiplier):
        speed_multiplier = 1
        out_sample_count = int(self.file_buffer_length / speed_multiplier)
        out_indices = np.linspace(0, self.file_buffer_length, out_sample_count)

        play_buffer = np.array(
            np.interp(out_indices, self.file_buffer_indices, self.file_buffer),
            dtype=self.buffer_data_type,
        )

        audio_sample = audiocore.RawSample(play_buffer, sample_rate=self.rate)
        player.play(audio_sample)


class SamplePlayer:
    def __init__(self, audio) -> None:

        sample_names = [
            "samples/snare_44k_16.wav",
            "samples/snare_44k_16.wav",
            "samples/snare_44k_16.wav",
            "samples/snare_44k_16.wav",
        ]

        self.samples = list(map(MonoSample, sample_names))
        self.sample_count = len(self.samples)

        first_sample = self.samples[0]
        self.mixer = audiomixer.Mixer(
            voice_count=4,
            bits_per_sample=first_sample.bits_per_sample,
            sample_rate=first_sample.rate,
            channel_count=1,
            samples_signed=first_sample.is_signed(),
        )

        audio.play(self.mixer)

        for voice in self.mixer.voice:
            voice.level = 0.8

    def play_sample(self, sample_index: int, speed_multiplier: float):
        index = sample_index % self.sample_count
        sample = self.samples[index]
        voice = self.mixer.voice[index]
        sample.play_at_speed(voice, 1)
