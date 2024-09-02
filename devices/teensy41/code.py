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

WITH_MEMORY_METRICS = False


class Tracker:
    def __init__(self, name):
        self.name = name
        self.reset()

    def start(self):
        self.start_ns = time.monotonic_ns()
        if WITH_MEMORY_METRICS:
            self.start_memory = gc.mem_alloc()

    def stop(self):
        self.count += 1
        time_diff = time.monotonic_ns() - self.start_ns
        if WITH_MEMORY_METRICS:
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


def run_pitchshift_demo(audio):
    # wave_file = adafruit_wave.open("samples/snare.wav")
    # wave_file = adafruit_wave.open("samples/snare.wav")
    wave_file = adafruit_wave.open("samples/open_hh.wav")
    channels, bit_depth, sample_rate = (
        wave_file.getnchannels(),
        wave_file.getsampwidth() * 8,
        wave_file.getframerate(),
    )

    assert channels == 1
    # assert bit_depth == 8
    print(f"bit_depth: {bit_depth}")

    mixer = audiomixer.Mixer(
        bits_per_sample=bit_depth,
        voice_count=4,
        sample_rate=sample_rate,
        channel_count=1,
        samples_signed = bit_depth == 16,
    )

    sample_bits = np.int16 if bit_depth == 16 else np.uint8

    audio.play(mixer)

    frame_count = wave_file.getnframes()
    print(f"frames: {frame_count}")
    file_buffer = np.frombuffer(wave_file.readframes(2*1024), dtype=sample_bits)
    file_buffer_length = len(file_buffer)
    
    file_buffer_indices = np.arange(file_buffer_length)

    def play_at_speed(speed_multiplier):
        out_sample_count = int(file_buffer_length / speed_multiplier)
        out_indices = np.linspace(0, file_buffer_length, out_sample_count)

        play_buffer = np.array(
            np.interp(out_indices, file_buffer_indices, file_buffer),
            dtype=sample_bits,
        )

        audio_sample = audiocore.RawSample(play_buffer, sample_rate=sample_rate)

        voice = mixer.voice[0]
        voice.play(audio_sample)

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
            play_at_speed(speed_multiplier)
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
