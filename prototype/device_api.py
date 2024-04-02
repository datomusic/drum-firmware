from drum import Drum
from note_output import NoteOutput

class DeviceAPI:
    def __init__(self):
        raise NotImplementedError("Required device method")

    def show(self, drum):
        raise NotImplementedError("Required device method")

    def handle_input(self, drum: Drum, note_out: NoteOutput):
        raise NotImplementedError("Required device method")
        
