import unittest
from unittest.mock import Mock, call
from firmware.device_api import Output
from firmware.multi_methods import MultiMethods


class OutputsTest(unittest.TestCase):
    def test_redirects(self):
        output1 = Mock(Output)
        output2 = Mock(Output)

        multi = MultiMethods([output1, output2])
        multi.send_note_on(1, 2, 3)

        self.assertEqual(
            output1.send_note_on.mock_calls,
            [call(1, 2, 3)])

        self.assertEqual(
            output2.send_note_on.mock_calls,
            [call(1, 2, 3)])


if __name__ == '__main__':
    unittest.main()
