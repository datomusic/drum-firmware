from device_api import DeviceAPI, PotName


class LocalDevDevice(DeviceAPI):
    def __init__(self):
        self.pot_counter = 0

    def update(self):
        pass

    def read_pot(self, pot: PotName):
        return 65536
        # if self.pot_counter > 100:
        #     self.pot_counter = 0
        # else:
        #     self.pot_counter += 1

        # return self.pot_counter * 100

    def send_note_on(self, note, vel):
        print(f"note_on: {note}, {vel}")

    def send_note_off(self, note):
        print(f"note_off: {note}")

    def show(self, drum):
        pass
