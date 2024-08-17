class Sample:
    def __init__(self, path):
        # Load sample data into numpy array and get recorded sample rate
        data, self.rate = load_wav(path)
        self.frequency = 100

        # Calculate tuning factor of note
        self.period = 1.0 / self.frequency # duration of a single wavelength
        self.duration = len(data) / self.rate # duration of entire sample
        self.tune = math.log(self.period / self.duration) / math.log(2)
        self.loop_tune = 0.0

        # Create note object
        self.notenum = -1
        self.note = synthio.Note(
            frequency = self.frequency,
            waveform = data,
            amplitude = 1,
            bend = self.tune
        )

        self._set_loop(0.0, 1.0)
        
    def _set_loop(self, start=0.0, end=1.0):
        if type(start) is float:
            start = int(len(self.note.waveform) * start)
        start = min(max(start, 0), len(self.note.waveform) - 1)

        if type(end) is float:
            end = int(len(self.note.waveform) * end)
        end = min(max(end, 1), len(self.note.waveform))
        
        length = end - start
        if length < 2:
            return
        
        self._loop_start = start
        self.note.waveform_loop_start = self._loop_start

        self._loop_end = end
        self.note.waveform_loop_end = self._loop_end
        
        self.loop_tune = math.log(len(self.note.waveform) / length) / math.log(2)
        self.note.bend = self.tune + self.loop_tune
        
        self.note.envelope = synthio.Envelope(
            attack_time = 0.0,
            attack_level = 1.0,
            decay_time = 0.0,
            sustain_level = 1.0,
            release_time = (len(self.note.waveform) - self._loop_end) / self.rate
        )

    def press(self, synth, notenum):
        # If the note is currently playing or releasing, force it to stop
        state, value = synth.note_info(self.note)
        if not state is None:
            if not state is synthio.EnvelopeState.RELEASE:
                synth.release(self.note)
            # Set note's release time to 0s
            self.note.envelope = synthio.Envelope(
                attack_time = 0.0,
                attack_level = 1.0,
                decay_time = 0.0,
                sustain_level = 1.0,
                release_time = 0.0
            )
            # Wait for note to be fully released
            while True:
                state, value = synth.note_info(self.note)
                if not state is None:
                    time.sleep(0.001)
                else:
                    break
            # Reset note release time
            self.note.envelope = synthio.Envelope(
                attack_time = 0.0,
                attack_level = 1.0,
                decay_time = 0.0,
                sustain_level = 1.0,
                release_time = (len(self.note.waveform) - self._loop_end) / self.rate
            )

        self.notenum = notenum
        self.note.waveform_loop_start = self._loop_start
        self.note.waveform_loop_end = self._loop_end
        self.note.frequency = synthio.midi_to_hz(notenum)
        self.note.bend = self.tune + self.loop_tune # Include loop tuning factor
        synth.press(self.note)

    def release(self, synth, notenum):
        if self.notenum != notenum:
            return False
        # Let the sample play to the end of the sample
        self.note.waveform_loop_start = 0
        self.note.waveform_loop_end = synthio.waveform_max_length
        self.note.bend = self.tune # Remove loop tuning factor
        synth.release(self.note)
        return True


def load_wav(path):
    data = None
    sample_rate = 0

    with adafruit_wave.open(path, "rb") as wave:
        if wave.getsampwidth() != 2 or wave.getnchannels() > 2:
            return False
        sample_rate = wave.getframerate()

        # Read sample and convert to numpy
        frames = min(wave.getnframes(), synthio.waveform_max_length)
        data = list(memoryview(wave.readframes(frames)).cast('h'))
        if wave.getnchannels() == 2: # Filter out right channel
            data = [i for i in range(0, frames, 2)]
        data = numpy.array(data, dtype=numpy.int16)

        # Normalize volume
        data = normalize(data)

    return data, sample_rate


def normalize(data):
    max_level = numpy.max(data)
    if max_level < 32767.0:
        for i in range(len(data)):
            data[i] = int(min(max(float(data[i]) * 32767.0 / max_level, -32767.0), 32767.0))
    return data
