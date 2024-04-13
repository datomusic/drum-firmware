from .drum import Drum

# Protocol


class Output:
    def __init__(self):
        pass

    def send_note_on(self, note, vel):
        raise NotImplementedError("Required method")

    def send_note_off(self, note):
        raise NotImplementedError("Required method")


class Controller:
    def __init__(self):
        raise NotImplementedError("Required method")

    def update(self, drum: Drum, output: Output):
        raise NotImplementedError("Required method")

    def show(self, drum: Drum):
        raise NotImplementedError("Required method")


class PotName:
    Speed = 1
    Volume = 2


# class DeviceCallbacks:
#     def __init__(self, on_clock):
#         self.on_clock = on_clock


class DeviceAPI:
    def update(self):
        raise NotImplementedError("Required device method")

    def read_pot(self, pot_name: PotName) -> int | None:
        raise NotImplementedError("Required device method")

    def send_note_on(self, note, vel):
        raise NotImplementedError("Required device method")

    def send_note_off(self, note):
        raise NotImplementedError("Required device method")

    def show(self, drum: Drum):
        raise NotImplementedError("Required device method")

    # def set_callbacks(self, callbacks: DeviceCallbacks):
    #     raise NotImplementedError("Required device method")

    # def handle_input(self, drum: Drum, note_out: NoteOutput):
    #     raise NotImplementedError("Required device method")

    # def get_midi_message(self):
    #     raise NotImplementedError("Required device method")
