from tempo import Tempo
from drum import Drum
from device_api import DeviceAPI
from midi import open_midi, get_midi_note_out

USE_INTERNAL_TEMPO = True


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


def run_application(device: DeviceAPI) -> None:
    drum = Drum()
    setup_tracks(drum.tracks)
    midi = open_midi()
    note_out = get_midi_note_out(midi)

    def on_tempo_tick():
        drum.tick(note_out.play)
        note_out.tick()

    tempo = Tempo(midi, on_tempo_tick)
    tempo.use_internal = USE_INTERNAL_TEMPO

    while True:
        msg = midi.receive()
        if msg:
            tempo.on_midi_msg(msg)

        tempo.update()
        device.handle_input(drum, note_out)
        device.show(drum)
