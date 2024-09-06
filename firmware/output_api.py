def _not_implemented(name, *args):
    print("[Unimplemented]", name, args)
    # raise NotImplementedError("Required method: " + name)


class OutputParam:
    Volume = 1
    LowPass = 2
    HighPass = 3
    AdjustFilter = 4
    Tempo = 5
    Distortion = 6
    Bitcrusher = 7


class Output:
    def send_note_on(self, channel: int, note: int, velocity_percent: float):
        _not_implemented("Output.send_note_on", note, velocity_percent)

    def send_note_off(self, channel: int, note: int):
        _not_implemented("Output.send_note_off", note)

    def set_lowpass_filter(self, amount_percent):
        _not_implemented("Output.set_lowpass_filter", amount_percent)

    def set_highpass_filter(self, amount_percent):
        _not_implemented("Output.set_highpass_filter", amount_percent)

    def set_channel_pitch(self, channel: int, pitch: float):
        _not_implemented("Output.set_channel_pitch", channel, pitch)

    def set_param(self, param, percent: float):
        _not_implemented("Output.set_param", param, percent)

    def on_tempo_tick(self, source):
        _not_implemented("Output.on_tempo_tick", source)

    def set_channel_mute(self, channel: int, amount_percent: float):
        _not_implemented("Output.set_channel_mute", channel, amount_percent)
