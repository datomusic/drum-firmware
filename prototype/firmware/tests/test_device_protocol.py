import unittest
import struct
from unittest.mock import Mock, call
from firmware.device_protocol import _handle_setting_action, Action, ByteReader
from firmware.settings import Settings


# Packs a 16bit unsigned integer into 3 7-bit bytes.
def pack3_16(value):
    return struct.pack(
        ">BBB",
        (value >> 14) & 0x7F,
        (value >> 7) & 0x7F,
        value & 0x7F,
    )


# Packs a 32bit unsigned integer into 6 7-bit bytes.
def pack3_32(value):
    top = value >> 16
    bottom = (value & 0xFFFF)

    return struct.pack(
        ">BBBBBB",
        (top >> 14) & 0x7F,
        (top >> 7) & 0x7F,
        top & 0x7F,
        (bottom >> 14) & 0x7F,
        (bottom >> 7) & 0x7F,
        bottom & 0x7F,
    )


class TestDeviceProtocol(unittest.TestCase):
    def test_sets_setting(self) -> None:
        send_response = Mock()
        settings = Mock(Settings)
        setting_id = 1  # Should be device.cursor_color
        value = 0xFF00FF
        message = pack3_16(setting_id) + pack3_32(value)
        _handle_setting_action(Action.SetSetting, ByteReader(message), settings, send_response)
        self.assertEqual(settings.set.mock_calls, [call("device.cursor_color", value)])

    def test_gets_setting(self) -> None:
        send_response = Mock()
        settings = Mock(Settings)
        settings.configure_mock(**{"get.return_value": 123})
        setting_id = 1  # Should be device.cursor_color
        message = pack3_16(setting_id)
        _handle_setting_action(Action.GetSetting, ByteReader(message), settings, send_response)
        self.assertEqual(settings.get.mock_calls, [call("device.cursor_color")])
        self.assertEqual(send_response.mock_calls, [call(struct.pack(">HI", setting_id, 123))])


if __name__ == '__main__':
    unittest.main()
