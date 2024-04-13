from note_output import NoteOutput
from tempo import Tempo
from drum import Drum
from device_api import DeviceAPI, PotName

USE_INTERNAL_TEMPO = False

BPM_MAX = 500
POT_MIN = 0
POT_MAX = 65536


def setup_tracks(tracks):
    tracks[0].note = 0
    tracks[1].note = 7
    tracks[2].note = 15
    tracks[3].note = 23

    tracks[0].sequencer.set_step(0)
    tracks[0].sequencer.set_step(4)
    tracks[1].sequencer.set_step(3)
    tracks[1].sequencer.set_step(5)
    tracks[2].sequencer.set_step(7)
    tracks[3].sequencer.set_step(6)


class PotReader:
    def __init__(self, pot_name, inverted=True):
        self.pot_name = pot_name
        self.last_val = None
        self.inverted = inverted

    def read(self, device):
        val = device.read_pot(self.pot_name)
        if val != self.last_val:
            if self.inverted:
                val = POT_MAX - val

            self.last_val = val
            # print(f"self.last_val: {self.last_val}")
            return (True, val)
        else:
            return (False, val)


def bpm_from_pot(pot_value):
    return ((POT_MAX - pot_value) / POT_MAX) * BPM_MAX


class Application:
    def __init__(self, device: DeviceAPI):
        self.device = device
        self.drum = Drum()
        setup_tracks(self.drum.tracks)

        self.note_out = NoteOutput(device.send_note_on, device.send_note_off)
        self.speed_pot = PotReader(PotName.Speed)

        def on_tempo_tick():
            self.drum.tick(self.note_out.play)
            self.note_out.tick()

        self.tempo = Tempo(on_tempo_tick)
        self.tempo.use_internal = USE_INTERNAL_TEMPO

    def update(self) -> None:
        self.device.update()
        self.__read_pots()
        # msg = self.device.get_midi_message()
        # if msg:
        #     self.tempo.on_midi_msg(msg)

        self.tempo.update()

    def show(self) -> None:
        self.device.show(self.drum)

    def __read_pots(self):
        (speed_changed, speed) = self.speed_pot.read(self.device)
        if speed_changed:
            self.tempo.set_bpm(bpm_from_pot(speed))


def run_application(device: DeviceAPI) -> None:
    app = Application(device)

    while True:
        app.update()
        app.show()
