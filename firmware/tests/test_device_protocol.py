import unittest
import struct
from unittest.mock import Mock, call
from firmware.device_protocol import _handle_setting_action, Tag
from firmware.settings import Settings


class TestDeviceProtocol(unittest.TestCase):
    def test_sets_setting(self) -> None:
        send_response = Mock()
        settings = Mock(Settings)
        setting_id = 1  # Should be device.cursor_color
        value = 0xFF00FF
        message = struct.pack(">HI", setting_id, value)
        _handle_setting_action(Tag.SetSetting, message, settings, send_response)
        self.assertEqual(settings.set.mock_calls, [call("device.cursor_color", value)])

    def test_gets_setting(self) -> None:
        send_response = Mock()
        settings = Mock(Settings)
        settings.configure_mock(**{"get.return_value": 123})
        setting_id = 1  # Should be device.cursor_color
        message = struct.pack(">H", setting_id)
        _handle_setting_action(Tag.GetSetting, message, settings, send_response)
        self.assertEqual(settings.get.mock_calls, [call("device.cursor_color")])
        self.assertEqual(send_response.mock_calls, [call(struct.pack(">HI", setting_id, 123))])


if __name__ == '__main__':
    unittest.main()
