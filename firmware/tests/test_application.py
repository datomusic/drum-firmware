import unittest
from unittest.mock import Mock
from firmware.application import Application
from firmware.controller_api import Controller
from firmware.settings import Settings


class DummySettings(Settings):
    def get(self, item):
        return 1


class ApplicationTest(unittest.TestCase):
    def test_application_single_step(self) -> None:
        controller = Mock(Controller)
        Application(controller, (None, None), DummySettings()).loop_step()


if __name__ == '__main__':
    unittest.main()
