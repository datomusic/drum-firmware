import unittest
from unittest.mock import Mock, call
from firmware.combined_methods import CombinedMethods


class Dummy:
    def __init__(self):
        pass

    def call_dummy(*args):
        pass


class CombinedMethodsTest(unittest.TestCase):
    def test_redirects(self):
        mock1 = Mock(Dummy)
        mock2 = Mock(Dummy)

        combined = CombinedMethods(Dummy(), [mock1, mock2])
        combined.call_dummy(1, 2, 3)

        self.assertEqual(
            mock1.call_dummy.mock_calls,
            [call(1, 2, 3)])

        self.assertEqual(
            mock2.call_dummy.mock_calls,
            [call(1, 2, 3)])


if __name__ == '__main__':
    unittest.main()
