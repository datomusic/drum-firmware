from .drum import Drum


def _not_implemented(name, *args):
    print("[Unimplemented]", name, args)
    # raise NotImplementedError("Required method: " + name)


class Output:
    def __init__(self):
        pass

    def send_note_on(self, note, vel):
        _not_implemented("send_note_on", note, vel)

    def send_note_off(self, note):
        _not_implemented("send_note_off", note)

    def adjust_filter(self, value):
        _not_implemented("adjust_filter", value)


class Controller:
    def update(self, drum: Drum, output: Output):
        _not_implemented("update")

    def show(self, drum: Drum):
        _not_implemented("show")
