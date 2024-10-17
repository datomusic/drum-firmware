import unittest
from unittest.mock import Mock
from firmware.application import Application
from firmware.controller_api import Controller
from firmware.settings import Settings
from adafruit_midi import MIDI
from firmware.output_api import Output


class DummySettings(Settings):
    def get(self, item):
        return 1


class DummyMIDI(MIDI):
    def __init__(self):
        pass

    def receive(self):
        return None


class ApplicationTest(unittest.TestCase):
    def test_application_single_step(self) -> None:
        controller = Mock(Controller)
        dummy_output = Mock(Output)
        Application(controller, dummy_output, DummyMIDI(), DummySettings()).loop_step()


if __name__ == '__main__':
    unittest.main()
