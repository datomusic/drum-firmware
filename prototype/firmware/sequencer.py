STEP_COUNT = 8


class Step:
    def __init__(self):
        self.velocity = 100.0
        self.active = False


class Sequencer:
    def __init__(self, play_step_callback):
        self.steps = [Step() for _ in range(STEP_COUNT)]
        self.cur_step_index = 0
        self.play_step_callback = play_step_callback

    def tick(self) -> None:
        step = self.steps[self.cur_step_index]
        if step.active:
            self.play_step_callback(step.velocity)
        self.cur_step_index = (self.cur_step_index + 1) % STEP_COUNT

    def set_step(self, index, velocity=100.0):
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
