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
