class Tempo:
    def __init__(self, ticks_per_beat):
        self.ticks_per_beat = ticks_per_beat
        self.ticks = 0

    def tick(self, on_beat):
        if self.ticks % self.ticks_per_beat == 0:
            self.ticks = 0
            on_beat()
        self.ticks += 1

    def reset(self):
        self.ticks = 0
