import unittest
from unittest.mock import Mock
from firmware.application import Application, AppControls
from firmware.tempo import Tempo
from firmware.device_api import Output, TrackParam, OutputParam, EffectName
from firmware.controller_api import Controller
from firmware.drum import Drum


class ApplicationTest(unittest.TestCase):
    def test_application_runs(self):
        output = Mock(Output)
        controller = Mock(Controller)
        Application(controller, output).run_iterator().__next__()


class AppControlsTest(unittest.TestCase):
    def test_app_controls(self):
        output = Mock(Output)
        drum = Drum(output, 1)
        tempo = Tempo(
            tempo_tick_callback=lambda _unused: None,
            on_quarter_beat=lambda _unused: None
        )
        on_sample_trigger = Mock()

        controls = AppControls(drum, output, tempo, on_sample_trigger)
        controls.set_bpm(100)
        controls.toggle_track_step(0, 0)
        controls.change_sample(0, 1)
        controls.set_playing(True)
        self.assertEqual(True, controls.is_playing())
        controls.play_track_sample(0, 100)
        controls.set_track_repeat_velocity(0, 100)
        controls.set_track_param(TrackParam.Pitch, 0, 100)
        controls.set_track_param(TrackParam.Mute, 0, 100)
        controls.set_output_param(OutputParam.AdjustFilter, 100)
        controls.set_effect_level(EffectName.Random, 100)
        controls.adjust_swing(1)
        controls.set_swing(100)
        controls.clear_swing()
        controls.handle_midi_clock()


if __name__ == '__main__':
    unittest.main()
