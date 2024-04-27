from .tempo import Divider, TICKS_PER_BEAT


class RepeatEffect:
    def __init__(self, cur_step_getter):
        self.repeat_count = 0
        self.start_step = None
        self.cur_step_getter = cur_step_getter
        self.step_counter = 0
        self.repeat_ticks = Divider(TICKS_PER_BEAT)

    def active(self):
        return self.repeat_count > 0

    def tick(self):
        should_step = self.repeat_ticks.tick()
        if self.repeat_count > 0:
            if should_step:
                self.step_counter += 1
                return True
        else:
            return False

    def set_repeat_divider(self, divider):
        self.repeat_ticks.set_division(TICKS_PER_BEAT / divider)

    def set_repeat_count(self, count):
        self.repeat_count = count

        if count <= 0:
            self.start_step = None
        elif self.start_step is None:
            self._start()

    def _start(self):
        self.start_step = (self.cur_step_getter)()
        self.step_counter = 0

    def get_step(self):
        if self.start_step is None:
            return None
        else:
            return (self.start_step - (self.repeat_count - (self.step_counter % self.repeat_count)))
