import keypad
import board
import neopixel
import analogio as aio


class SequencerKey:
    def __init__(self, step, track = 0) -> None:
        self.track: int = track
        self.step: int = step


class KeyboardKey:
    def __init__(self, number) -> None:
        self.number: int = number


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
    def __init__(self,
                 key: SequencerKey | KeyboardKey | ControlKey,
                 pressed: bool) -> None:
        self.pressed = pressed
        self.key = key


class PotEvent:
    def __init__(self, value) -> None:
        self.value: int = value


class Controls:
    def __init__(self):
        self.keys = init_keymatrix()
        self.pot1 = aio.AnalogIn(board.POT_1)
        self.pot2 = aio.AnalogIn(board.POT_2)

    def get_key_event(self) \
            -> KeyEvent | None:
        key_event = self.keys.events.get()
        if key_event:
            return translate_key_event(key_event)

        return None


def init_keymatrix():
    row_pins = (
        board.ROW_1,
        board.ROW_2,
        board.ROW_3,
        board.ROW_4,
    )

    col_pins = (
        board.LED_1,
        board.LED_3,
        board.COL_1,
        board.COL_2,
        board.COL_3,
        board.COL_4,
        board.COL_5,
        board.COL_6,
    )

    return keypad.KeyMatrix(
        row_pins=row_pins,
        column_pins=col_pins,
        interval=0.05
    )


def translate_key_event(event) -> KeyEvent | None:
    key: SequencerKey | KeyboardKey | ControlKey | None = None
    n = event.key_number
    
    print(n)

    if n < 8:
        key = SequencerKey(n, 0)
    elif n < 16:
        key = SequencerKey(n % 8, 1)
    elif n < 24:
        key = SequencerKey(n % 8, 2)
    elif n < 32:
        key = SequencerKey(n % 8, 3)


    if key:
        return KeyEvent(key, event.pressed)

    return None


step_to_led = {
    0: 4,
    1: 5,
    2: 13,
    3: 14,
    4: 22,
    5: 23,
    6: 31,
    7: 32,

    8: 3,
    9: 6,
    10: 12,
    11: 15,
    12: 21,
    13: 24,
    14: 30,
    15: 33,

    16: 2,
    17: 7,
    18: 11,
    19: 16,
    20: 20,
    21: 25,
    22: 29,
    23: 34,

    24: 1,
    25: 8,
    26: 10,
    27: 17,
    28: 19,
    29: 26,
    30: 28,
    31: 35
}


class Display:
    def __init__(self):
        self.pixels = init_pixels()
        self.show = self.pixels.show

    def clear(self):
        self.pixels.fill((0, 0, 0))

    def set_color(self,
                  key: KeyboardKey | SequencerKey | ControlKey,
                  color: None | tuple[int, int, int]) -> None:
        if not color:
            color = (0, 0, 0)

        if isinstance(key, SequencerKey):
            self.pixels[step_to_led[key.step + (key.track * 8)]] = color
        elif isinstance(key, KeyboardKey):
            self.pixels[9] = color
        elif isinstance(key, ControlKey) and key.name == ControlName.Start:
            self.pixels[0] = color


def init_pixels():
    pixel_count = 36

    pixels = neopixel.NeoPixel(
        board.NEOPIXEL, pixel_count, brightness=0.3, auto_write=False
    )

    pixels.fill((0, 0, 0))
    return pixels
