import board
import audiocore
import ulab.numpy as np
import adafruit_wave
import audiopwmio
import audiomixer

wave_file = adafruit_wave.open("samples/boink.wav")
channels, bit_depth, sample_rate = (
    wave_file.getnchannels(),
    wave_file.getsampwidth() * 8,
    wave_file.getframerate(),
)

assert channels == 1
assert bit_depth == 8

audio = audiopwmio.PWMAudioOut(board.D12)
mixer = audiomixer.Mixer(
    samples_signed=False,
    bits_per_sample=8,
    voice_count=4,
    sample_rate=sample_rate,
    channel_count=1,
)

audio.play(mixer)
file_buffer = wave_file.readframes(1024)
file_buffer_length = len(file_buffer)


def play_downpitched_wav(speed_multiplier):
    buffer_length = int(file_buffer_length / speed_multiplier)
    play_buffer = np.zeros(buffer_length, dtype=np.uint8)

    for i in range(buffer_length):
        ind = int(i * speed_multiplier)
        play_buffer[i] = file_buffer[ind]

    audio_sample = audiocore.RawSample(
        bytearray(play_buffer.tobytes()), sample_rate=sample_rate
    )

    voice = mixer.voice[0]
    voice.play(audio_sample)


import time

while True:
    time.sleep(0.5)
    play_downpitched_wav(0.6)
