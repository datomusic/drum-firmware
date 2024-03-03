import keypad
import board
import neopixel
import analogio as aio


class SequencerKey:
    def __init__(self, step) -> None:
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
        board.COL_1,
        board.COL_2,
        board.COL_3,
        board.COL_4,
        board.COL_5,
        board.COL_6,
    )

    return keypad.KeyMatrix(
        row_pins=col_pins,
        column_pins=row_pins,
        interval=0.05
    )


def translate_key_event(event) -> KeyEvent | None:
    key: SequencerKey | KeyboardKey | ControlKey | None = None
    n = event.key_number

    if n == 0:
        key = ControlKey(ControlName.Seq1)
    # Skip n == 1
    elif n == 2:
        key = KeyboardKey(0)
    elif n == 3:
        key = KeyboardKey(6)
    elif n == 4:
        key = SequencerKey(7)
    elif n == 5:
        key = ControlKey(ControlName.Start)
    elif n == 6:
        key = ControlKey(ControlName.Down)
    elif n == 7:
        key = KeyboardKey(5)
    elif n == 8:
        key = SequencerKey(0)
    elif n == 9:
        key = SequencerKey(1)
    elif n == 10:
        key = KeyboardKey(2)
    elif n == 11:
        key = KeyboardKey(8)
    elif n == 12:
        key = ControlKey(ControlName.Seq2)
    elif n == 13:
        key = SequencerKey(2)
    elif n == 14:
        key = KeyboardKey(1)
    elif n == 15:
        key = KeyboardKey(7)
    elif n == 16:
        key = SequencerKey(6)
    elif n == 17:
        key = SequencerKey(3)
    elif n == 18:
        key = KeyboardKey(4)
    elif n == 19:
        key = ControlKey(ControlName.Up)
    elif n == 20:
        key = SequencerKey(5)
    elif n == 21:
        key = SequencerKey(4)
    elif n == 22:
        key = KeyboardKey(3)
    elif n == 23:
        key = KeyboardKey(9)

    if key:
        return KeyEvent(key, event.pressed)

    return None


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
            self.pixels[key.step+1] = color
        elif isinstance(key, KeyboardKey):
            self.pixels[key.number + 9] = color
        elif isinstance(key, ControlKey) and key.name == ControlName.Start:
            self.pixels[0] = color


def init_pixels():
    pixel_count = 19

    pixels = neopixel.NeoPixel(
        board.NEOPIXEL, pixel_count, brightness=0.3, auto_write=False
    )

    pixels.fill((0, 0, 0))
    return pixels
