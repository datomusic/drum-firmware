import time
import gc

WITH_MEMORY_METRICS = False


class Metrics:
    def __init__(self):
        self.loop_tracker = Tracker("loop")
        self.loop_nanoseconds = 0
        self.functions = []

    def begin_loop(self) -> None:
        self.loop_tracker.start()

    def end_loop(self, delta_nanoseconds: int) -> None:
        self.loop_tracker.stop()
        self.loop_nanoseconds += delta_nanoseconds

        if self.loop_nanoseconds > 1_000_000_000:
            self.loop_nanoseconds = 0
            self._report()

    def wrap(self, name: str, function):
        timed = Timed(name, function)
        self.functions.append(timed)
        return timed

    def _report(self):
        print(self.loop_tracker.get_info())
        self.loop_tracker.reset()
        for timed in self.functions:
            print(timed.tracker.get_info())
            timed.tracker.reset()
        print()


class Tracker:
    def __init__(self, name):
        self.name = name
        self.reset()

    def start(self):
        self.start_ns = time.monotonic_ns()
        if WITH_MEMORY_METRICS:
            self.start_memory = gc.mem_alloc()

    def stop(self):
        self.count += 1
        time_diff = time.monotonic_ns() - self.start_ns
        if WITH_MEMORY_METRICS:
            memory_diff = gc.mem_alloc() - self.start_memory
        else:
            memory_diff = 0

        if self.average_ns > 0:
            self.average_ns = (self.average_ns + time_diff) / 2
            self.average_memory = (self.average_memory + memory_diff) / 2
            self.min = min(self.min, time_diff)
            self.max = max(self.max, time_diff)
        else:
            self.average_ns = time_diff
            self.average_memory = memory_diff
            self.min = time_diff
            self.max = time_diff

        self.total_ns += time_diff

    def reset(self):
        self.count = 0
        self.start_ns = 0
        self.total_ns = 0
        self.min = 0
        self.max = 0
        self.average_ns = 0
        self.average_memory = 0
        self.start_memory = 0

    def get_info(self):
        return f"\
[{self.name}] count: {self.count}, \
avg: {self.average_ns / 1_000_000:.2f}ms, \
min: {self.min / 1_000_000:.2f}ms, \
max: {self.max / 1_000_000:.2f}ms, \
alloc: {self.average_memory}"


class Timed:
    def __init__(self, name, function):
        self.tracker = Tracker(name)
        self.function = function
        self.name = name

    def __call__(self, *args):
        self.tracker.start()
        self.function(*args)
        self.tracker.stop()
