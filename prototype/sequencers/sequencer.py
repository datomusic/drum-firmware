STEP_COUNT = 8


class Step:
    def __init__(self):
        self.velocity = 0
        self.note = 0
        self.active = 0


class Sequencer:
    def __init__(self):
        self.steps = [Step() for _ in range(STEP_COUNT)]
        self.cur_step_index = 0

    def tick(self, play_note) -> None:
        play_step(self.steps[self.cur_step_index], play_note)
        self.cur_step_index = (self.cur_step_index + 1) % STEP_COUNT

    def step_on(self, index, note=None, velocity=127):
        step = self.steps[index]
        step.active = not step.active
        if not note == None:
            step.note = note
            step.velocity = velocity


def play_step(step, play_note):
    if step.active:
        play_note(step.note, step.velocity)
