class RepeatEffect:
    def __init__(self, cur_step_getter):
        self.repeat_count = 0
        self.start_step = None
        self.cur_step_getter = cur_step_getter
        self.counter = 0

    def tick(self):
        self.counter += 1

    def set_repeat_count(self, count):
        self.repeat_count = count

        if count <= 0:
            self.start_step = None
        elif self.start_step is None:
            self._start()

    def _start(self):
        self.start_step = (self.cur_step_getter)()
        self.counter = 0

    def get_step(self):
        if self.start_step is None:
            return None
        else:
            return self.start_step + (self.counter % self.repeat_count)
