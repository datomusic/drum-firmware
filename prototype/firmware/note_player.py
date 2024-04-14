NOTE_LENGTH = 2  # ticks


class NotePlayer:
    def __init__(self, send_note_on, send_note_off):
        self.ticks = 0
        self.note = None
        self.send_note_on = send_note_on
        self.send_note_off = send_note_off

    def play(self, note, vel=100.0):
        if self.note is not None:
            self.send_note_off(self.note)

        self.send_note_on(note, vel)
        self.ticks = 0
        self.note = note

    def tick(self):
        if self.note is not None:
            self.ticks += 1
            if self.ticks >= NOTE_LENGTH:
                self.send_note_off(self.note)
                self.ticks = 0
                self.active = False
