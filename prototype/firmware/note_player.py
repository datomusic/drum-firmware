from .device_api import Output

NOTE_LENGTH = 1  # ticks


class NotePlayer:
    def __init__(self, channel: int, output: Output):
        self.played_note: int | None
        self.played_note = None

        self.channel = channel
        self.ticks = 0
        self.output = output

    def play(self, note: int, velocity: float = 100.0) -> None:
        if self.played_note is not None:
            self.output.send_note_off(self.channel, self.played_note)

        self.output.send_note_on(self.channel, note, velocity)
        self.ticks = 0

    def playing(self):
        return self.played_note is not None

    def tick(self) -> None:
        if self.played_note is not None:
            self.ticks += 1
            if self.ticks >= NOTE_LENGTH:
                self.output.send_note_off(self.channel, self.played_note)
                self.played_note = None
                self.ticks = 0
