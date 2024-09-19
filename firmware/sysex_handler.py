from .settings import Settings
import struct

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


class SysexHandler:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings

    def handle_sysex_data(self, message_bytes) -> None:
        print("Received sysex:", message_bytes)
        length = len(message_bytes)
        if length < 2:
            return

        (tag,) = struct.unpack("H", message_bytes[:2])
        print("Tag:", tag)

        if tag == Tag.SetSetting:
            (setting_index, value) = struct.unpack("HH", message_bytes[2:4])
            if setting_index < __SETTINGS_COUNT:
                setting_name = __SETTING_NAMES[setting_index]
                self.settings.set(setting_name, value)
        elif tag == Tag.GetSetting:
            (setting_index, value) = struct.unpack("HH", message_bytes[2:4])
            if setting_index < __SETTINGS_COUNT:
                setting_name = __SETTING_NAMES[setting_index]
                return struct.pack("HH", setting_index, self.settings.get(setting_name))
