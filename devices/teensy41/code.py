import board
import audiocore
import ulab.numpy as np
import adafruit_wave
import audiomixer
import usb_midi
import adafruit_midi
from adafruit_midi.note_on import NoteOn
import time
import gc


class Tracker:
    WITH_MEMORY_METRICS = False

    def __init__(self, name):
        self.name = name
        self.reset()

    def start(self):
        self.start_ns = time.monotonic_ns()
        if Tracker.WITH_MEMORY_METRICS:
            self.start_memory = gc.mem_alloc()

    def stop(self):
        self.count += 1
        time_diff = time.monotonic_ns() - self.start_ns
        if Tracker.WITH_MEMORY_METRICS:
            memory_diff = gc.mem_alloc() - self.start_memory
        else:
            memory_diff = 0

        if self.average_ns > 0:
            self.average_ns = (self.average_ns + time_diff) / 2
            self.average_memory = (self.average_memory + memory_diff) / 2
            self.min = min(self.min, time_diff)
            self.max = max(self.max, time_diff)
        else:
            self.average_ns = time_diff
            self.average_memory = memory_diff
            self.min = time_diff
            self.max = time_diff

        self.total_ns += time_diff

    def reset(self):
        self.count = 0
        self.start_ns = 0
        self.total_ns = 0
        self.min = 0
        self.max = 0
        self.average_ns = 0
        self.start_memory = 0

    def get_info(self):
        return f"[{self.name}] count: {self.count}, avg: {self.average_ns / 1_000_000:.2f}ms, min: {self.min / 1_000_000:.2f}ms, max: {self.max / 1_000_000:.2f}ms, alloc: {self.average_memory}"


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
        out_sample_count = int(self.file_buffer_length / speed_multiplier)
        out_indices = np.linspace(0, self.file_buffer_length, out_sample_count)

        play_buffer = np.array(
            np.interp(out_indices, self.file_buffer_indices, self.file_buffer),
            dtype=self.buffer_data_type,
        )

        audio_sample = audiocore.RawSample(
            play_buffer, sample_rate=self.rate)

        player.play(audio_sample)


def run_pitchshift_demo(audio):
    sample = MonoSample("samples/snare_44k_16.wav")

    mixer = audiomixer.Mixer(
        voice_count=4,
        bits_per_sample=sample.bits_per_sample,
        sample_rate=sample.rate,
        channel_count=1,
        samples_signed=sample.is_signed(),
    )

    audio.play(mixer)

    midi = adafruit_midi.MIDI(midi_in=usb_midi.ports[0], in_channel=0)
    loop_tracker = Tracker("loop")
    last_ns = time.monotonic_ns()

    accumulated_info_ns = 0
    while True:
        loop_tracker.start()
        now = time.monotonic_ns()
        delta_ns = now - last_ns
        last_ns = now
        accumulated_info_ns += delta_ns
        msg = midi.receive()

        if isinstance(msg, NoteOn) and msg.velocity != 0:
            print(f"Note On: {msg.note}")
            speed_multiplier = 2 * (msg.note / 127)
            # speed_multiplier = 1
            # print(f"speed_multiplier: {speed_multiplier}")

            sample.play_at_speed(mixer.voice[0], speed_multiplier)

        loop_tracker.stop()
        if accumulated_info_ns > 1_000_000_000:
            accumulated_info_ns = 0
            print(loop_tracker.get_info())
            loop_tracker.reset()
            print()


WITH_PWM = True

if WITH_PWM:
    import audiopwmio

    audio = audiopwmio.PWMAudioOut(board.D12)
    run_pitchshift_demo(audio)
else:
    import adafruit_wm8960
    import audiobusio

    i2s = audiobusio.I2SOut(
        board.AUDIO_TX_BCLK,
        board.AUDIO_TX_SYNC,
        board.AUDIO_TXD,
        main_clock=board.AUDIO_MCLK,
    )
    dac = adafruit_wm8960.WM8960(board.I2C())
    run_pitchshift_demo(i2s)
