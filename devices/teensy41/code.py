import board
import audiocore
import ulab.numpy as np
import adafruit_wave
import audiomixer


def run_pitchshift_demo(audio):
    wave_file = adafruit_wave.open("samples/boink.wav")
    channels, bit_depth, sample_rate = (
        wave_file.getnchannels(),
        wave_file.getsampwidth() * 8,
        wave_file.getframerate(),
    )

    assert channels == 1
    assert bit_depth == 8

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
    file_buffer_indices = np.arange(file_buffer_length)

    def play_downpitched_wav(speed_multiplier):
        out_sample_count = int(file_buffer_length / speed_multiplier)
        out_indices = np.linspace(0, file_buffer_length, out_sample_count)

        play_buffer = np.array(
            np.interp(out_indices, file_buffer_indices, file_buffer),
            dtype=np.uint8,
        )

        audio_sample = audiocore.RawSample(
            bytearray(play_buffer.tobytes()), sample_rate=sample_rate
        )

        voice = mixer.voice[0]
        voice.play(audio_sample)

    import time

    while True:
        time.sleep(0.5)
        play_downpitched_wav(0.6)


WITH_PWM = True

if WITH_PWM:
    import audiopwmio
    audio = audiopwmio.PWMAudioOut(board.D12)
    run_pitchshift_demo(audio)
else:
    import adafruit_wm8960
    import audiobusio

    i2s = audiobusio.I2SOut(board.AUDIO_TX_BCLK, board.AUDIO_TX_SYNC, board.AUDIO_TXD, main_clock=board.AUDIO_MCLK)
    dac = adafruit_wm8960.WM8960(board.I2C())
    run_pitchshift_demo(i2s)
