from firmware.tempo import Swing, TICKS_PER_BEAT
import unittest


class SwingTest(unittest.TestCase):
    def test_quarter_beats_order(self):
        for amount in range(-Swing.Range, Swing.Range):
            swing = Swing()
            swing.set_amount(amount)
            quarters = []

            def on_quarter(index):
                quarters.append(index)

            for i in range(TICKS_PER_BEAT):
                swing.tick(1, on_quarter)

            self.assertEqual([0, 1, 2, 3], quarters)


if __name__ == '__main__':
    unittest.main()
