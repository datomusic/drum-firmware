def _not_implemented(name, *args):
    print("[Unimplemented]", name, args)
    # raise NotImplementedError("Required method: " + name)


class OutputParam:
    Volume = 1
    LowPass = 2
    HighPass = 3
    AdjustFilter = 4


class EffectName:
    Repeat = 1
    Random = 2


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


class Controls:
    def set_output_param(self, param, amount_percent):
        _not_implemented("Controls.set_output_param", param, amount_percent)

    def set_bpm(self, bpm):
        _not_implemented("Controls.set_bpm", bpm)

    def play_track_sample(self, track_index: int, velocity_percent: float):
        _not_implemented("Controls.play_track_sample",
                         track_index, velocity_percent)

    def set_track_mute(self, track_index: int, amount_percent: float):
        _not_implemented("Controls.set_track_mute",
                         track_index, amount_percent)

    def toggle_track_step(self, track_index: int, step):
        _not_implemented("Controls.toggle_track_step", track_index, step)

    def set_track_pitch(self, track_index: int, pitch_percent: float):
        _not_implemented("Controls.set_track_pitch",
                         track_index, pitch_percent)

    def change_sample(self, track_index: int, change):
        _not_implemented("Controls.change_sample", track_index, change)

    def toggle_playing(self):
        _not_implemented("Controls.toggle_playing")

    def adjust_swing(self, amount_percent):
        _not_implemented("Controls.adjust_swing", amount_percent)

    def clear_swing(self):
        _not_implemented("Controls.clear_swing")


class SampleChange:
    Next = 1
    Prev = -1
