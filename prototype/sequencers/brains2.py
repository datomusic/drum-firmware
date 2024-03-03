import keypad
import board
import neopixel

# import digitalio
# Pull = digitalio.Pull

button_names = (
    'BTN_SEQ1', 'DUMMY_KEY', 'KEYB_0', 'KEYB_6',
    'STEP_8', 'SEQ_START', 'BTN_DOWN', 'KEYB_5',
    'STEP_1', 'STEP_2', 'KEYB_2', 'KEYB_8',
    'BTN_SEQ2', 'STEP_3', 'KEYB_1', 'KEYB_7',
    'STEP_7', 'STEP_4', 'KEYB_4', 'BTN_UP',
    'STEP_6', 'STEP_5', 'KEYB_3', 'KEYB_9'
)


class Keys:
    def __init__(self):
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

        self.keys = keypad.KeyMatrix(
            row_pins=col_pins,
            column_pins=row_pins,
            interval=0.1
        )

        self.keys.reset()

    def get_event(self):

        event = self.keys.events.get()
        if event:
            if event.key_number < len(button_names):
                return (event.pressed, button_names[event.key_number])


num_pixels = 19
pixels = neopixel.NeoPixel(
    board.NEOPIXEL, num_pixels, brightness=0.3, auto_write=False)

pixels.fill((1, 1, 1))

pixels[2] = (250, 0, 0)
# cols = [digitalio.DigitalInOut(x) for x in col_pins]
# rows = [digitalio.DigitalInOut(x) for x in row_pins]

# button_rows = (
#     ("BTN_SEQ1",  "STEP_8",    "STEP_1",   "BTN_SEQ2", "STEP_7", "STEP_6"),
#     ("DUMMY_KEY", "SEQ_START", "STEP_2",   "STEP_3",   "STEP_4", "STEP_5"),
#     ("KEYB_0",    "BTN_DOWN",  "KEYB_2",   "KEYB_1",   "KEYB_4", "KEYB_3"),
#     ("KEYB_6",    "KEYB_5",    "KEYB_8",   "KEYB_7",   "BTN_UP", "KEYB_9")
# )

# def scan_keys(on_down):
#     for row in rows:
#         row.switch_to_input(pull=Pull.UP)

#     for (col_ind, col) in enumerate(cols):
#         col.switch_to_output()
#         col.value = False
#         time.sleep(0.005)

#         for (row_ind, row) in enumerate(rows):
#             if not row.value:
#                 on_down(row_ind, col_ind)
#         col.switch_to_input(pull=Pull.UP)
