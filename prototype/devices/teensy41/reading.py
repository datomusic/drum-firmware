BPM_MAX = 500
POT_MIN = 0
POT_MAX = 65536


def bpm_from_pot(pot_value):
    return ((POT_MAX - pot_value) / POT_MAX) * BPM_MAX


class IncDecReader:
    def __init__(self, dec_pin, inc_pin):
        self.was_zero = True
        self.inc_pin = inc_pin
        self.dec_pin = dec_pin

    def read(self, on_changed):
        threshold = 1000
        val = self.dec_pin.value
        if val > threshold:
            self.was_zero = False
            on_changed(- val)
        else:
            val = self.inc_pin.value
            if val > threshold:
                self.was_zero = False
                on_changed(val)
            elif not self.was_zero:
                on_changed(0)
                self.was_zero = True


class PotReader:
    def __init__(self, pin, inverted=True):
        self.pin = pin
        self.inverted = inverted
        self.last_val = None

    def read(self, on_changed):
        tolerance = 100

        val = self.pin.value
        if self.last_val is None or abs(val - self.last_val) > tolerance:
            self.last_val = val

            if self.inverted:
                val = POT_MAX - val

            on_changed(val)
            return True
        else:
            return False
