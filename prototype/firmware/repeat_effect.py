
class RepeatEffect:
    def __init__(self, cur_step_getter):
        self.final_step = None
        self.repeat_count = 0
        self.cur_step_getter = cur_step_getter
        self.step_counter = 0

    def active(self):
        return self.repeat_count > 0

    def advance(self):
        if self.active():
            self.step_counter += 1

    def set_subdivision(self, divider):
        # TODO:  Implement subdivision
        pass

    def set_repeat_count(self, count):
        self.repeat_count = count

        if count <= 0:
            self.final_step = None
        elif self.final_step is None:
            self._start()

    def _start(self):
        self.final_step = (self.cur_step_getter)()
        self.step_counter = 0

    # Plays final_step first, just after triggering.
    # Then loops from repeat_count steps back.
    def get_step(self, offset=0) -> None | int:
        if self.final_step is None:
            return None
        else:
            start = self.final_step - self.repeat_count + 1
            loop_step = ((self.step_counter + self.repeat_count - 1 + offset)
                         % self.repeat_count)

            return start + loop_step
