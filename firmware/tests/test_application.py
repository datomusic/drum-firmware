import unittest
from unittest.mock import Mock
from firmware.application import Application
from firmware.device_api import Output


class TestSettings:
    def get(self, item):
        return 1


class ApplicationTest(unittest.TestCase):
    def test_application_runs(self):
        output = Mock(Output)
        Application([], output, TestSettings()).run_iterator().__next__()


if __name__ == '__main__':
    unittest.main()
