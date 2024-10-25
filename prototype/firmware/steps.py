class Step:
    def __init__(self):
        self.velocity = 100.0
        self.active = False
        self.enabled = True


class Steps:
    def __init__(self, step_count):
        self.entries = [Step() for _ in range(step_count)]

    def set_step(self, index, velocity=100.0, enabled=True):
        step = self.entries[index]
        if velocity > 0:
            step.velocity = velocity
            step.active = True
        else:
            step.active = False
            step.velocity = 0
        step.enabled = enabled

    def toggle_step(self, index) -> bool:
        step = self.entries[index]
        step.active = not step.active
        return step.active

    def shift_steps(self, offset):
        length = len(self.entries)
        offset = offset % length
        self.entries = self.entries[-offset:] + self.entries[:-offset]
