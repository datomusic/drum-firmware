import unittest
from unittest.mock import Mock
from firmware.application import Application
from firmware.controller_api import Controller
from firmware.settings import Settings
from adafruit_midi import MIDI


class DummySettings(Settings):
    def get(self, item):
        return 1
        Application(controller, output).run_iterator().__next__()



class DummyMIDI(MIDI):
    def __init__(self):
        pass

    def receive(self):
        return None


class ApplicationTest(unittest.TestCase):
    def test_application_single_step(self) -> None:
        controller = Mock(Controller)
        Application(controller, DummyMIDI(), DummySettings()).loop_step()


if __name__ == '__main__':
    unittest.main()
