from .drum import Drum


def _not_implemented(name, *args):
    print("[Unimplemented]", name, args)
    # raise NotImplementedError("Required method: " + name)


class Output:
    def send_note_on(self, note, vel):
        _not_implemented("Output.send_note_on", note, vel)

    def send_note_off(self, note):
        _not_implemented("Output.send_note_off", note)

    def set_filter(self, value):
        _not_implemented("Output.set_filter", value)

    def set_channel_pitch(self, channel_index, pitch):
        _not_implemented("Output.set_channel_pitch", channel_index, pitch)


class Controls:
    def adjust_filter(self, value):
        _not_implemented("Controls.adjust_filter", value)

    def set_bpm(self, bpm):
        _not_implemented("Controls.set_bpm", bpm)

    def set_volume(self, vol):
        _not_implemented("Controls.set_volume", vol)

    def toggle_track_step(self, track_index, step):
        _not_implemented("Controls.toggle_track_step", track_index, step)

    def set_track_pitch(self, track_index, pitch):
        _not_implemented("Controls.set_track_pitch", track_index, pitch)

    def change_sample(self, track_index: int, change):
        _not_implemented("Controls.change_sample", track_index, change)

    def toggle_playing(self):
        _not_implemented("Controls.toggle_playing")


class SampleChange:
    Next = 1
    Prev = -1


class Controller:
    def update(self, controls: Controls):
        _not_implemented("Controller.update")

    def show(self, drum: Drum):
        _not_implemented("Controller.show")
