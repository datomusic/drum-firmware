class Step:
    def __init__(self):
        self.velocity = 100.0
        self.active = False


class Sequencer:
    def __init__(self, step_count):
        self.steps = [Step() for _ in range(step_count)]

    def set_step(self, index, velocity=100.0):
        step = self.steps[index]
        if velocity > 0:
            step.velocity = velocity
            step.active = True
        else:
            step.active = False
            step.velocity = 0

    def toggle_step(self, index) -> bool:
        step = self.steps[index]
        step.active = not step.active
        return step.active
