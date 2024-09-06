import unittest
from unittest.mock import Mock, call
from firmware.output_api import Output
from firmware.sequencer import Sequencer


class TestSettings:
    def get(self, item):
        return 1


class SequencerTest(unittest.TestCase):
    def test_no_play_when_repeat_active(self):
        output = Mock(Output)
        sequencer = Sequencer(output=output, track_count=1,
                              settings=TestSettings())
        track = sequencer.tracks[0]
        self.assertEqual(True, sequencer.playing)
        velocity = 100
        track.repeat_velocity = velocity
        sequencer.on_quarter_beat(0)
        sequencer.on_quarter_beat(1)

        track.play(50)

        sequencer.on_quarter_beat(2)
        sequencer.on_quarter_beat(3)

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
        sequencer = Sequencer(output=output, track_count=1,
                              settings=TestSettings())
        track = sequencer.tracks[0]
        self.assertEqual(True, sequencer.playing)
        repeat_velocity_1 = 98
        repeat_velocity_2 = 99
        play_velocity = 50

        track.play(play_velocity)

        track.repeat_velocity = repeat_velocity_1
        # Repeat should not trigger, since manual play happened recently
        sequencer.on_quarter_beat(0)

        # Repeat should trigger here
        track.repeat_velocity = repeat_velocity_2
        sequencer.on_quarter_beat(1)

        self.assertEqual(
            output.send_note_on.mock_calls,
            [call(0, 0, play_velocity),
                call(0, 0, repeat_velocity_2),
             ])

    def test_plays_track_if_no_track_repeat(self):
        output = Mock(Output)
        sequencer = Sequencer(output=output, track_count=1,
                              settings=TestSettings())
        track = sequencer.tracks[0]
        self.assertEqual(True, sequencer.playing)
        track.repeat_velocity = 100
        sequencer.on_quarter_beat(0)
        # Should not play since repeat is active
        track.play(40)
        output.send_note_on.assert_called_once_with(0, 0, 100)
        track.repeat_velocity = 0

        # Tick one quarter with repeat off
        sequencer.on_quarter_beat(1)

        # Note should play
        track.play(50)

        self.assertEqual(
            output.send_note_on.mock_calls,
            [call(0, 0, 100),
                call(0, 0, 50),
             ])


if __name__ == '__main__':
    unittest.main()
