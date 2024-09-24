import struct
import adafruit_logging as logging
from .settings import Settings

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)  # type: ignore

__SETTING_NAMES = [
    "device.brightness",
    "device.cursor_color",

    # Per track settings
    "track.0.init_note",
    "track.0.init_pattern",
    "track.0.range",
    "track.0.midi_channel",

    "track.1.init_note",
    "track.1.init_pattern",
    "track.1.range",
    "track.1.midi_channel",

    "track.2.init_note",
    "track.2.init_pattern",
    "track.2.range",
    "track.2.midi_channel",

    "track.3.init_note",
    "track.3.init_pattern",
    "track.3.range",
    "track.3.midi_channel",

    # Note mappings
    "note.0.color",
    "note.1.color",
    "note.2.color",
    "note.3.color",
    "note.4.color",
    "note.5.color",
    "note.6.color",
    "note.7.color",
    "note.8.color",
    "note.9.color",
    "note.10.color",
    "note.11.color",
    "note.12.color",
    "note.13.color",
    "note.14.color",
    "note.15.color",
    "note.16.color",
    "note.17.color",
    "note.18.color",
    "note.19.color",
    "note.20.color",
    "note.21.color",
    "note.22.color",
    "note.23.color",
    "note.24.color",
    "note.25.color",
    "note.26.color",
    "note.27.color",
    "note.28.color",
    "note.29.color",
    "note.30.color",
    "note.31.color"
]

__SETTINGS_COUNT = len(__SETTING_NAMES)


class Tag:
    SetSetting = 1
    GetSetting = 2
    RequestVersion = 3


class DeviceProtocol:
    def __init__(self, response_sender, settings: Settings) -> None:
        self.settings = settings
        self.response_sender = response_sender

    def handle_message(self, message: bytes) -> None:
        if len(message) < 2:
            logger.error("Message too short")
            return

        (tag,) = struct.unpack(">H", message[:2])

        if tag in [Tag.SetSetting, Tag.GetSetting]:
            _handle_setting_action(tag, message[2:], self.settings, self.response_sender)
        elif tag == Tag.RequestVersion:
            pass  # TODO


def _handle_setting_action(tag: int, message: bytes, settings: Settings, response_sender) -> None:
    (setting_index,) = struct.unpack(">H", message[:2])
    if setting_index < __SETTINGS_COUNT:
        setting_name = __SETTING_NAMES[setting_index]
    else:
        logger.error("Invalid setting index")
        return

    if tag == Tag.SetSetting:
        (value, ) = struct.unpack(">I", message[2:6])
        settings.set(setting_name, value)

    elif tag == Tag.GetSetting:
        value = settings.get(setting_name)
        if not isinstance(value, int):
            logger.error("Setting value is not an integer")
        else:
            content_bytes = struct.pack(">HI", setting_index, value)
            response_sender(content_bytes)
