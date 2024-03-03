import keypad
import board
import neopixel
from enum import Enum, auto
from copy import copy


class KeyEvent:
    def __init__(self, pressed=False) -> None:
        self.pressed: bool = pressed


class SequencerKey(KeyEvent):
    def __init__(self, step) -> None:
        self.step: int = step


class KeyboardKey(KeyEvent):
    def __init__(self, number) -> None:
        self.number: int = number


class ControlName(Enum):
    Seq1 = auto()
    Seq2 = auto()
    Start = auto()
    Down = auto()
    Up = auto()


class ControlKey(KeyEvent):
    def __init__(self, name) -> None:
        self.name: ControlName = name


class PotEvent:
    def __init__(self, value) -> None:
        self.value: int = value


class Controls:
    def __init__(self):
        self.keys = init_keymatrix()

    def get_event(self) \
            -> SequencerKey | KeyboardKey | ControlKey | PotEvent | None:
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
        interval=0.1
    )


KEY_MAPPINGS = (
    ControlKey("Seq1"),
    None,
    KeyboardKey(0),
    KeyboardKey(6),
    SequencerKey(8),
    ControlKey("Start"),
    ControlKey("Down"),
    KeyboardKey(5),
    SequencerKey(1),
    SequencerKey(2),
    KeyboardKey(2),
    KeyboardKey(8),
    ControlKey("Seq2"),
    SequencerKey(3),
    KeyboardKey(1),
    KeyboardKey(7),
    SequencerKey(7),
    SequencerKey(4),
    KeyboardKey(4),
    ControlKey("Up"),
    SequencerKey(6),
    SequencerKey(5),
    KeyboardKey(3),
    KeyboardKey(9),
)

KEYMAP_LEN = len(KEY_MAPPINGS)


def translate_key_event(event) \
        -> SequencerKey | KeyboardKey | ControlKey | None:
    if event.key_number >= 0 and event.key_number < KEYMAP_LEN:
        key = copy(KEY_MAPPINGS[event.key_number])
        if key:
            key.pressed = event.pressed
            return key

    return None


class Display:
    def __init__(self):
        self.pixels = init_pixels()


def init_pixels():
    pixel_count = 19

    pixels = neopixel.NeoPixel(
        board.NEOPIXEL, pixel_count, brightness=0.3, auto_write=False
    )

    pixels.fill((1, 1, 1))
    return pixels
