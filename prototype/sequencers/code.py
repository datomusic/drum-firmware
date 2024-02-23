from drum import Drum
import usb_midi

usb_midi.enable()

drum = Drum()
drum.run()
