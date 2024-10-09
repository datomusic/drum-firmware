from firmware.output_api import OutputParam, OutputChannelParam
from firmware.settings import Settings
from firmware.controller_api import Controller
from firmware.drum import Drum, TRACK_COUNT
from .pizza_view import PizzaView

from .hardware import (
    Teensy41Hardware,
    SequencerKey,
    SampleSelectKey,
    Direction,
    ControlKey,
    ControlName,
)

from .reading import (
    PotReader,
    IncDecReader,
    DigitalTrigger,
    ThresholdTrigger,
    DigitalChanger,
    percentage_from_pot,
)

BPM_MIN = 30
BPM_MAX = 360


class DrumPad:
    def __init__(self, track_index, trigger_port, mute_port) -> None:
        self.track_index = track_index
        self.trigger = ThresholdTrigger(trigger_port)
        self.mute = PotReader(mute_port)
        self.muted_when_triggered = False

    def update(self, drum: Drum, view: PizzaView) -> None:
        was_triggered = self.trigger.triggered
        triggered, value = self.trigger.read()
        trigger_changed = was_triggered != self.trigger.triggered

        if not self.trigger.triggered or self.muted_when_triggered:
            self.mute.read(
                lambda amount: drum.output.set_channel_param(
                    self.track_index,
                    OutputChannelParam.Mute,
                    percentage_from_pot(amount),
                )
            )

        velocity = percentage_from_pot(value)
        muted = self.mute.last_val > 1000

        if trigger_changed:
            if self.trigger.triggered:
                self.muted_when_triggered = muted
            else:
                drum.sequencer.tracks[self.track_index].repeat_velocity = 0
                self.muted_when_triggered = False

        elif velocity > 1:
            drum.sequencer.tracks[self.track_index].repeat_velocity = velocity

        if triggered:
            drum.sequencer.tracks[self.track_index].play(velocity)
            view.trigger_track(self.track_index)


class PizzaController(Controller):
    def __init__(self, settings: Settings, hardware=None) -> None:
        if hardware is None:
            hardware = Teensy41Hardware()

        self.hardware = hardware
        self.settings = settings
        # TODO: bounds setting on brightness setting
        brightness = int(settings.get("device.brightness")) / 256
        self.display = hardware.init_display(brightness)
        self.view = PizzaView(TRACK_COUNT, settings)

        self.speed_setting = PotReader(self.hardware.speed_pot)
        self.volume_setting = PotReader(self.hardware.volume_pot)

        self.filter_setting = IncDecReader(
            self.hardware.filter_left, self.hardware.filter_right
        )

        self.lowpass_setting = PotReader(self.hardware.filter_left)
        self.highpass_setting = PotReader(self.hardware.filter_right)
        self.beat_repeat_setting = PotReader(self.hardware.repeat_button)
        self.random_setting = PotReader(self.hardware.random_button)
        self.highpass_setting = PotReader(self.hardware.filter_right)

        self.swing_left = DigitalTrigger(self.hardware.swing_left)
        self.distortion = DigitalChanger(self.hardware.swing_left)
        self.swing_right = DigitalTrigger(self.hardware.swing_right)
        self.bitcrusher = DigitalChanger(self.hardware.swing_right)

        self.pitch_settings = [
            PotReader(self.hardware.pitch1),
            PotReader(self.hardware.pitch2),
            PotReader(self.hardware.pitch3),
            PotReader(self.hardware.pitch4),
        ]

        self.drum_pads = [
            DrumPad(0, self.hardware.drum_pad1,
                    self.hardware.drum_pad1_bottom),
            DrumPad(1, self.hardware.drum_pad2,
                    self.hardware.drum_pad2_bottom),
            DrumPad(2, self.hardware.drum_pad3,
                    self.hardware.drum_pad3_bottom),
            DrumPad(3, self.hardware.drum_pad4,
                    self.hardware.drum_pad4_bottom),
        ]

    def fast_update(self, drum: Drum, _delta_ms: int) -> None:
        for track_index, pad in enumerate(self.drum_pads):
            pad.update(drum, self.view)

    def update(self, drum: Drum, delta_ms: int) -> None:
        self._read_pots(drum)
        self._process_keys(drum)

    def show(self, drum: Drum, delta_ms: int, beat_position: float) -> None:
        self.view.update(delta_ms)
        self.display.clear()
        self.view.show(self.display, drum.sequencer, beat_position)
        self.display.show()

    def _read_pots(self, drum: Drum) -> None:
        out_param = OutputParam()

        def set_speed(pot_speed):
            speed = percentage_from_pot(pot_speed)
            range = BPM_MAX - BPM_MIN
            drum.tempo.set_bpm((speed * range / 100) + BPM_MIN)
            drum.output.set_param(out_param.Tempo, speed)

        self.speed_setting.read(set_speed)

        # self.volume_setting.read(
        #     lambda vol: drum.output.set_param(
        #         out_param.Volume,
        #         percentage_from_pot(vol)))
        self.volume_setting.read(
            lambda volume: drum.tempo.swing.set_amount(
                int(6 - (volume / (65536 / 12))))
        )

        self.filter_setting.read(
            lambda val: drum.output.set_param(
                out_param.AdjustFilter, percentage_from_pot(val) / 50
            )
        )

        self.lowpass_setting.read(
            lambda val: drum.output.set_param(
                out_param.LowPass, percentage_from_pot(val)
            )
        )

        self.highpass_setting.read(
            lambda val: drum.output.set_param(
                out_param.HighPass, percentage_from_pot(val)
            )
        )

        self.distortion.read(
            lambda val: drum.output.set_param(
                out_param.Distortion, val * 100)
        )

        self.bitcrusher.read(
            lambda val: drum.output.set_param(
                out_param.Bitcrusher, val * 100)
        )

        self.highpass_setting.read(
            lambda val: drum.output.set_param(
                out_param.HighPass, percentage_from_pot(val)
            )
        )

        self.beat_repeat_setting.read(
            lambda val: drum.sequencer.set_repeat_effect_level(
                percentage_from_pot(val))
        )

        self.random_setting.read(
            lambda val: drum.sequencer.set_random_enabled(
                percentage_from_pot(val) > 50)
        )

        for track_index, pitch_setting in enumerate(self.pitch_settings):
            pitch_setting.read(
                lambda pitch: drum.output.set_channel_param(
                    track_index,
                    OutputChannelParam.Pitch,
                    percentage_from_pot(pitch)
                )
            )

    def _process_keys(self, drum: Drum) -> None:
        event = self.hardware.get_key_event()
        pressed = event and event.pressed
        if pressed:
            key = event.key
            if isinstance(key, SequencerKey):
                drum.sequencer.toggle_track_step(key.track, key.step)

            elif isinstance(key, SampleSelectKey):
                if key.direction == Direction.Down:
                    change = 1
                elif key.direction == Direction.Up:
                    change = -1

                drum.sequencer.change_sample(key.track, change)

            elif isinstance(key, ControlKey):
                if key.name == ControlName.Start:
                    drum.sequencer.set_playing(not drum.sequencer.is_playing())
