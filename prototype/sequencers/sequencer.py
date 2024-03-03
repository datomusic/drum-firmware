STEP_COUNT = 8


class Step:
    def __init__(self):
        self.velocity = 127
        self.active = False


class Sequencer:
    def __init__(self):
        self.steps = [Step() for _ in range(STEP_COUNT)]
        self.cur_step_index = 0

    def tick(self, play_step_callback) -> None:
        play_step(self.steps[self.cur_step_index], play_step_callback)
        self.cur_step_index = (self.cur_step_index + 1) % STEP_COUNT

    def set_step(self, index, velocity=127):
        step = self.steps[index]
        if velocity > 0:
            step.velocity = velocity
            step.active = True
        else:
            step.active = False
            step.velocity = 0

    def toggle_step(self, index):
        step = self.steps[index]
        step.active = not step.active


def play_step(step, play_step_callback):
    if step.active:
        play_step_callback(step.velocity)
