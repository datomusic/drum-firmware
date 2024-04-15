def _not_implemented(name, *args):
    print("[Unimplemented]", name, args)
    # raise NotImplementedError("Required method: " + name)


class Output:
    def send_note_on(self, channel: int, note: int, velocity_percent: float):
        _not_implemented("Output.send_note_on", note, velocity_percent)

    def send_note_off(self, channel: int, note: int):
        _not_implemented("Output.send_note_off", note)

    def set_filter(self, value: float):
        _not_implemented("Output.set_filter", value)

    def set_channel_pitch(self, channel: int, pitch: float):
        _not_implemented("Output.set_channel_pitch", channel, pitch)

    def set_volume(self, vol: float):
        _not_implemented("Output.set_volume", vol)


class Controls:
    def adjust_filter(self, amount_percent):
        _not_implemented("Controls.adjust_filter", amount_percent)

    def set_bpm(self, bpm):
        _not_implemented("Controls.set_bpm", bpm)

    def set_volume(self, vol: float):
        _not_implemented("Controls.set_volume", vol)

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


class SampleChange:
    Next = 1
    Prev = -1
