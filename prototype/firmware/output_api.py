def _not_implemented(name, *args):
    print("[Unimplemented]", name, args)
    # raise NotImplementedError("Required method: " + name)


class OutputParam:
    def __init__(self):
        self.Volume = 1
        self.LowPass = 2
        self.HighPass = 3
        self.AdjustFilter = 4
        self.Tempo = 5
        self.Distortion = 6
        self.Bitcrusher = 7


class OutputChannelParam:
    Pitch = 1
    Mute = 2


class Output:
    def send_note_on(self, channel: int, note: int, velocity_percent: float):
        _not_implemented("Output.send_note_on", note, velocity_percent)

    def send_note_off(self, channel: int, note: int):
        _not_implemented("Output.send_note_off", note)

    def set_param(self, param, percent: float):
        _not_implemented("Output.set_param", param, percent)

    def set_channel_param(self, channel: int, param, percent: float):
        _not_implemented("Output.set_channel_param", param, channel, percent)

    def on_tempo_tick(self, source):
        _not_implemented("Output.on_tempo_tick", source)
