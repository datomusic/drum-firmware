from .device_api import Output

NOTE_LENGTH = 2  # ticks


class NotePlayer:
    def __init__(self, channel: int, output: Output):
        self.played_note: int | None
        self.played_note = None

        self.channel = channel
        self.ticks = 0
        self.output = output
        self.mute_level = 0.0

    def play(self, note: int, vel: float = 100.0) -> None:
        if self.played_note is not None:
            self.output.send_note_off(self.channel, self.played_note)

        self.output.send_note_on(self.channel, note, vel - self.mute_level)
        self.ticks = 0
        self.played_note = note

    def tick(self) -> None:
        if self.played_note is not None:
            self.ticks += 1
            if self.ticks >= NOTE_LENGTH:
                self.output.send_note_off(self.channel, self.played_note)
                self.played_note = None
                self.ticks = 0
