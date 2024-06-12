import unittest
from unittest.mock import Mock, call
from firmware.device_api import Output
from firmware.drum import Drum


class DrumTest(unittest.TestCase):
    def test_no_play_when_repeat_active(self):
        output = Mock(Output)
        drum = Drum(output=output, track_count=1)
        track = drum.tracks[0]
        self.assertEqual(True, drum.playing)
        velocity = 100
        track.repeat_velocity = velocity
        drum.on_quarter_beat(0)
        drum.on_quarter_beat(1)

        track.play(50)

        drum.on_quarter_beat(2)
        drum.on_quarter_beat(3)

        # Manually played note should not have triggered,
        # since repeat was active
        self.assertEqual(
            output.send_note_on.mock_calls,
            [call(0, 0, velocity),
                call(0, 0, velocity),
                call(0, 0, velocity),
                call(0, 0, velocity)
             ])

    def test_no_repeat_soon_after_play(self):
        output = Mock(Output)
        drum = Drum(output=output, track_count=1)
        track = drum.tracks[0]
        self.assertEqual(True, drum.playing)
        repeat_velocity_1 = 98
        repeat_velocity_2 = 99
        play_velocity = 50

        track.play(play_velocity)

        track.repeat_velocity = repeat_velocity_1
        # Repeat should not trigger, since manual play happened recently
        drum.on_quarter_beat(0)

        # Repeat should trigger here
        track.repeat_velocity = repeat_velocity_2
        drum.on_quarter_beat(1)

        self.assertEqual(
            output.send_note_on.mock_calls,
            [call(0, 0, play_velocity),
                call(0, 0, repeat_velocity_2),
             ])

    def test_plays_track_if_no_track_repeat(self):
        output = Mock(Output)
        drum = Drum(output=output, track_count=1)
        track = drum.tracks[0]
        self.assertEqual(True, drum.playing)
        track.repeat_velocity = 100
        drum.on_quarter_beat(0)
        # Should not play since repeat is active
        track.play(40)
        output.send_note_on.assert_called_once_with(0, 0, 100)
        track.repeat_velocity = 0

        # Tick one quarter with repeat off
        drum.on_quarter_beat(1)

        # Note should play
        track.play(50)

        self.assertEqual(
            output.send_note_on.mock_calls,
            [call(0, 0, 100),
                call(0, 0, 50),
             ])


if __name__ == '__main__':
    unittest.main()
