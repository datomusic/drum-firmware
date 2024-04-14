from .drum import Drum


def _not_implemented(name, *args):
    print("[Unimplemented]", name, args)
    # raise NotImplementedError("Required method: " + name)


class Output:
    def send_note_on(self, note, vel):
        _not_implemented("send_note_on", note, vel)

    def send_note_off(self, note):
        _not_implemented("send_note_off", note)

    def set_filter(self, value):
        _not_implemented("set_filter", value)


class Controls:
    def adjust_filter(self, value):
        _not_implemented("adjust_filter", value)

    def set_bpm(self, bpm):
        _not_implemented("set_bpm", bpm)

    def toggle_track_step(self, track, step):
        _not_implemented("toggle_track_step", track, step)

    def change_sample(self, track: int, change):
        _not_implemented("change_sample", track, change)

    def toggle_playing(self):
        _not_implemented("toggle_playing")


class SampleChange:
    Next = 1
    Prev = -1


class Controller:
    def update(self, controls: Controls):
        _not_implemented("update")

    def show(self, drum: Drum):
        _not_implemented("show")
