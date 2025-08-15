from .output_api import Output


class NotePlayer:
    def __init__(self, channel: int, output: Output):
        self.channel = channel
        self.output = output

    def play(self, note: int, velocity: float = 100.0) -> None:
        self.output.send_note_on(self.channel, note, velocity)
        self.output.send_note_off(self.channel, note)
