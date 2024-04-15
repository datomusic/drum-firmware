import time

import keypad  # type: ignore
import board  # type: ignore
import microcontroller  # type: ignore
import neopixel  # type: ignore
import analogio as aio  # type: ignore
import digitalio as dio  # type: ignore


class Direction:
    Up = 1
    Down = -1


class SequencerKey:
    def __init__(self, step, track=0) -> None:
        self.track: int = track
        self.step: int = step


class SampleSelectKey:
    def __init__(self, direction, track) -> None:
        self.direction: int = direction
        self.track: int = track


class Drumpad:
    def __init__(self, track) -> None:
        self.track: int = track


class ControlName:
    Seq1 = 1
    Seq2 = 2
    Start = 3
    Down = 4
    Up = 5


class ControlKey:
    def __init__(self, name) -> None:
        self.name: ControlName = name


class KeyEvent:
    def __init__(
        self, key: SequencerKey | SampleSelectKey | ControlKey, pressed: bool
    ) -> None:
        self.pressed = pressed
        self.key = key


class PotEvent:
    def __init__(self, value) -> None:
        self.value: int = value


class ThresholdButton:
    def __init__(self, pin, threshold):
        self.state = False
        self.pin = pin
        self.threshold = threshold

    def pressed(self) -> bool:
        val = self.pin.value
        print(f"val: {val}")

        if not self.state and val > self.threshold:
            self.state = True
            return True
        elif self.state and val < self.threshold:
            self.state = False

        return False


class ToggleButton:
    def __init__(self, pin, inverted=True):
        self.state = False
        self.pin = pin
        self.inverted = inverted

    def pressed(self) -> bool:
        dig = dio.DigitalInOut(self.pin)
        val = dig.value
        dig.deinit()

        if self.state != val:
            self.state = val
            ret = val
            if self.inverted:
                ret = not ret
            return ret

        return False


class AnalogReader:
    def __init__(self, pin):
        self.pin = pin

    def read(self):
        analog = aio.AnalogIn(self.pin)
        val = analog.value
        analog.deinit()
        return val


class Teensy41Hardware:
    def __init__(self):
        microcontroller.cpu.frequency = 150000000
        self.keys = init_keymatrix()
        self.repeat_button = AnalogReader(board.A0)
        self.pitch1 = AnalogReader(board.A1)

        self.drum_pad1 = AnalogReader(board.A2)
        self.drum_pad1_bottom = AnalogReader(board.A3)

        self.volume_pot = AnalogReader(board.A4)
        self.swing_right = ToggleButton(board.D8)
        self.swing_left = ToggleButton(board.D7)
        self.pitch2 = AnalogReader(board.A5)

        self.drum_pad2 = AnalogReader(board.A6)
        self.drum_pad2_bottom = AnalogReader(board.A7)

        self.random_button = AnalogReader(board.A8)
        self.pitch3 = AnalogReader(board.A9)

        self.drum_pad3 = AnalogReader(board.A10)
        self.drum_pad3_bottom = AnalogReader(board.A11)

        self.play_button = ToggleButton(board.D37)
        self.speed_pot = AnalogReader(board.A12)
        self.filter_right = AnalogReader(board.A13)
        self.filter_left = AnalogReader(board.D38)
        self.pitch4 = AnalogReader(board.D39)

        self.drum_pad4 = AnalogReader(board.D40)
        self.drum_pad4_bottom = AnalogReader(board.D41)

    def get_key_event(self) -> KeyEvent | None:
        key_event = self.keys.events.get()
        if key_event:
            return translate_key_event(key_event)
        else:
            if self.play_button.pressed():
                return KeyEvent(ControlKey(ControlName.Start), True)

        return None


def init_keymatrix():
    col_pins = (board.D3, board.D4, board.D5, board.D6, board.D9)

    row_pins = (
        board.D10,
        board.D11,
        board.D12,
        board.D28,
        board.D29,
        board.D30,
        board.D31,
        board.D32,
    )

    return keypad.KeyMatrix(
        row_pins=row_pins,
        column_pins=col_pins,
        interval=0.1,
        columns_to_anodes=False
    )


def translate_key_event(event) -> KeyEvent | None:
    key: SequencerKey | SampleSelectKey | ControlKey | None = None
    n = event.key_number

    if (n % 5) < 4:
        key = SequencerKey(int(n / 5), n % 5)
    elif n == 9:
        key = SampleSelectKey(Direction.Up, 3)
    elif n == 14:
        key = SampleSelectKey(Direction.Down, 3)
    elif n == 19:
        key = SampleSelectKey(Direction.Up, 2)
    elif n == 24:
        key = SampleSelectKey(Direction.Down, 2)
    elif n == 29:
        key = SampleSelectKey(Direction.Up, 1)
    elif n == 34:
        key = SampleSelectKey(Direction.Down, 1)
    elif n == 39:
        key = SampleSelectKey(Direction.Up, 0)
    elif n == 4:
        key = SampleSelectKey(Direction.Down, 0)
    else:
        key = None
        # key = ControlKey(n)

    if key:
        return KeyEvent(key, event.pressed)

    return None


step_to_led = {
    0: 1,
    1: 40,
    2: 31,
    3: 30,
    4: 21,
    5: 20,
    6: 11,
    7: 10,
    8: 2,
    9: 39,
    10: 32,
    11: 29,
    12: 22,
    13: 19,
    14: 12,
    15: 9,
    16: 3,
    17: 38,
    18: 33,
    19: 28,
    20: 23,
    21: 18,
    22: 13,
    23: 8,
    24: 4,
    25: 37,
    26: 34,
    27: 27,
    28: 24,
    29: 17,
    30: 14,
    31: 7,
}

drumpad_to_led = {0: 5, 1: 16, 2: 25, 3: 36}


class Display:
    def __init__(self):
        self.pixels = init_pixels()
        self.show = self.pixels.show

    def clear(self):
        self.pixels.fill((0, 0, 0))

    def set_color(
        self,
        key: Drumpad | SequencerKey | ControlKey,
        color: None | tuple[int, int, int],
    ) -> None:
        if not color:
            color = (0, 0, 0)

        if isinstance(key, SequencerKey):
            self.pixels[step_to_led[key.step + (key.track * 8)]] = color
        elif isinstance(key, Drumpad):
            self.pixels[drumpad_to_led[key.track]] = color
        elif isinstance(key, ControlKey) and key.name == ControlName.Start:
            self.pixels[0] = color


def init_pixels():
    pixel_count = 41

    pixels = neopixel.NeoPixel(
        board.D2, pixel_count, brightness=1.0, auto_write=False)

    for i in range(pixel_count):
        pixels[i] = (60, 60, 60)
        time.sleep(0.02)
        pixels.show()
    time.sleep(0.5)

    pixels.fill((0, 0, 0))
    return pixels
