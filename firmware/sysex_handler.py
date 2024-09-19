from .settings import Settings
from adafruit_midi.system_exclusive import SystemExclusive  # type: ignore


class SysexHandler:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings

    def handle_sysex(self, message: SystemExclusive) -> None:
        pass
