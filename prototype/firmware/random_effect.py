import random


class RandomEffect:
    def __init__(self, step_count):
        self.step_count = step_count
        self.enabled = False
        self.step = 0

    def tick(self):
        self.step = random.randint(0, self.step_count)

    def get_step(self):
        if self.enabled:
            return self.step
        else:
            return None
