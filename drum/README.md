# DATO DRUM MIDI Implementation Chart

## General Settings
- **Default MIDI Channel:** 10 (GM Percussion Standard), configurable via the `SetSetting` SysEx command (see Settings Commands below)
- **Note Off Messages:** Ignored
- **Velocity Range:** 1-127
- **Clock:** Master by default, slave when external clock detected

## MIDI Note Numbers (Sample Mapping)

### Track 1
| Sample # | MIDI Note | GM Percussion Name |
|----------|-----------|-------------------|
| 1 | 30 | - |
| 2 | 31 | - |
| 3 | 32 | - |
| 4 | 33 | - |
| 5 | 34 | - |
| 6 | 35 | Acoustic Bass Drum |
| 7 | 36 | Bass Drum |
| 8 | 37 | Side Stick |

### Track 2
| Sample # | MIDI Note | GM Percussion Name |
|----------|-----------|-------------------|
| 1 | 38 | Acoustic Snare |
| 2 | 39 | Hand Clap |
| 3 | 40 | Electric Snare |
| 4 | 41 | Low Floor Tom |
| 5 | 42 | Closed Hi Hat |
| 6 | 43 | High Floor Tom |
| 7 | 44 | Pedal Hi-Hat |
| 8 | 45 | Low Tom |

### Track 3
| Sample # | MIDI Note | GM Percussion Name |
|----------|-----------|-------------------|
| 1 | 46 | Open Hi-Hat |
| 2 | 47 | Low-Mid Tom |
| 3 | 48 | Hi-Mid Tom |
| 4 | 49 | Crash Cymbal 1 |
| 5 | 50 | High Tom |
| 6 | 51 | Ride Cymbal 1 |
| 7 | 52 | Chinese Cymbal |
| 8 | 53 | Ride Bell |

### Track 4
| Sample # | MIDI Note | GM Percussion Name |
|----------|-----------|-------------------|
| 1 | 54 | Tambourine |
| 2 | 55 | Splash Cymbal |
| 3 | 56 | Cowbell |
| 4 | 57 | Crash Cymbal 2 |
| 5 | 58 | Vibraslap |
| 6 | 59 | Ride Cymbal 2 |
| 7 | 60 | Hi Bongo |
| 8 | 61 | Low Bongo |

## MIDI Control Change (CC) Assignments

### Global Controls
| Control | CC Number | Value Range | Description |
|---------|-----------|-------------|-------------|
| Master Volume | 7 | 0-127 | Master output volume |
| Swing | 9 | 0-127 | Rhythm timing (64=no swing) |
| Crush Effect | 12 | 0-127 | Bitcrusher/distortion amount |
| Tempo | 15 | 0-127 | BPM control (when master) |
| Random Effect | 16 | 0-127 | Random step jumping |
| Repeat Effect | 17 | 0-127 | Current step repeat |
| Filter Cutoff | 74 | 0-127 | Low-pass filter frequency |
| Filter Resonance | 75 | 0-127 | Filter resonance amount |

### Per-Track Pitch Controls
| Track | CC Number | Value Range | Description |
|-------|-----------|-------------|-------------|
| Track 1 Pitch | 21 | 0-127 | ±1 octave (64=no change) |
| Track 2 Pitch | 22 | 0-127 | ±1 octave (64=no change) |
| Track 3 Pitch | 23 | 0-127 | ±1 octave (64=no change) |
| Track 4 Pitch | 24 | 0-127 | ±1 octave (64=no change) |

### Effect Button Pressure Mapping
| Pressure Level | CC Value | Description |
|----------------|----------|-------------|
| Not Pressed | 0 | Effect off |
| Light Press | 63 | Effect half intensity |
| Full Press | 127 | Effect full intensity |

## Drum Pad MIDI Output
- **Note On:** Velocity 1-127 based on initial strike pressure
- **Polyphonic Aftertouch (0xA0):** Continuous pressure while held
- **Note Number:** Current sample selection for that track
- **Hold Behavior:** Repeats notes at sequencer tempo
- **Hard Press:** Triggers at double tempo rate

## MIDI Clock & Transport

### Clock Behavior
- **Master Mode:** Sends MIDI Clock + Tempo CC 15
- **Slave Mode:** Follows incoming MIDI Clock (ignores CC 15)
- **Auto-Detection:** Switches to slave when external clock detected

### Transport Commands
| MIDI Message | Response |
|--------------|----------|
| Start (0xFA) | Begin/resume playback from current step |
| Continue (0xFB) | Begin/resume playback from current step |
| Stop (0xFC) | Stop playback, maintain step position |
| Song Position Pointer | Not implemented |

### Clock Output
- **24 PPQN** (Pulses Per Quarter Note)
- **Continuous when playing**
- **Stops when sequencer stopped**

## Configuration Notes
- The MIDI channel is configurable via the `SetSetting` SysEx command (see Settings Commands below)
- Sample-to-note mapping updates in real-time during sample selection
- **MIDI Input:** Receiving a note plays that sound on the corresponding track AND sets that note as the active sample for that track
- MIDI Clock input automatically detected and followed

## Build Script

Use `./build.sh` to build and upload firmware with A/B partition support:

```bash
./build.sh [OPTIONS]
```

**Common Options:**
- `-v, --verbose`: Enable verbose logging
- `-r, --ram`: Copy to RAM (default) 
- `-f, --flash`: Build for flash execution
- `-p N, --partition=N`: Upload to partition 0 (Firmware A) or 1 (Firmware B)
- `-c, --clean`: Remove build directory before building
- `-n, --no-upload`: Build only, don't upload
- `--setup-partitions`: Create and flash partition table from drum/partition_table.json
- `-h, --help`: Show all options

**Note:** The build script automatically attempts to force the device into BOOTSEL mode when needed.

**Examples:**
```bash
./build.sh                    # Default: RAM build, auto-upload
./build.sh -v -p 1           # Verbose, upload to Firmware B
./build.sh --clean --flash   # Clean flash build
```

## System Exclusive (SysEx) Commands

The DATO DRUM supports custom SysEx commands for advanced operations like file transfer and system management. All custom commands follow a specific format.

### Message Format
All custom SysEx messages share the following structure:
`F0 <Manufacturer ID> <Device ID> <Encoded Command> <Encoded Payload> F7`
- **F0**: Standard SysEx Start byte.
- **Manufacturer ID**: `00 22 01` (Dato Musical Instruments).
- **Device ID**: `65` (DRUM).
- **Encoded Command**: A 16-bit command tag, encoded into three 7-bit bytes.
- **Encoded Payload**: Command-specific data, with each pair of 8-bit bytes encoded into three 7-bit bytes.
- **F7**: Standard SysEx End byte.

A utility tool, `tools/drumtool/drumtool.js`, is provided to handle device communication and sample management (upload and download) using the MIDI Sample Dump Standard (SDS) protocol.

### General Commands
| Command | Tag | Description |
|---|---|---|
| RequestFirmwareVersion | 0x01 | Requests the device's firmware version. The device replies with a SysEx message containing the version (e.g., v0.2.0). |
| RequestSerialNumber | 0x02 | Requests the device's unique serial number. |
| RebootBootloader | 0x0B | Reboots the device into its USB bootloader mode for firmware updates. |
| FormatFilesystem | 0x15 | Erases and formats the internal filesystem. All stored samples and configuration will be lost. |

### File Transfer Protocol
Transferring files (like samples or `kit.bin`) to the device is a multi-step process managed by the `drumtool.js` script.

1.  **Begin Transfer:**
    - **Command:** `BeginFileWrite` (0x10)
    - **Payload:** The null-terminated filename (e.g., "kick.wav\0").
    - The sender sends this command to tell the device to open a file for writing. The device replies with an `Ack` (0x13) on success or `Nack` (0x14) on failure.

2.  **Send Data Chunks:**
    - **Command:** `FileBytes` (0x11)
    - **Payload:** A chunk of the file's binary data.
    - The sender splits the file into chunks and sends each one with this command. The device writes the data and sends an `Ack` for each chunk it receives successfully.

3.  **End Transfer:**
    - **Command:** `EndFileTransfer` (0x12)
    - **Payload:** None.
    - After sending all data chunks, the sender sends this command. The device closes the file, finalizing the write, and sends a final `Ack`.

### Sample Transfer (MIDI Sample Dump Standard)

Samples are transferred using the standard MIDI SDS protocol on SysEx channel
`65`, with the DRUM acting as receiver (upload) or sender (download).
`tools/drumtool/drumtool.js send` and `drumtool.js receive` are the reference
clients.

#### Upload (host → DRUM)
The host sends an SDS **Dump Header** (`F0 7E 65 01 ...`) describing a 16-bit
sample, followed by **Data Packets** (`F0 7E 65 02 ...`, 40 samples each).
The device ACKs the header and each packet, and stores the audio as raw
16-bit little-endian PCM in `/NN.pcm`, where `NN` is the two-digit sample
number from the header.

#### Download (DRUM → host)
The host requests a sample with an SDS **Dump Request**:

```
F0 7E 65 03 <sample # LSB> <sample # MSB> F7
```

- The device replies with a Dump Header for `/NN.pcm` and then streams data
  packets, waiting for the host's ACK/NAK/WAIT/CANCEL after the header and
  after every packet (NAK retransmits, CANCEL aborts). If a handshaking
  host's response goes missing, the device retransmits the same packet
  (every 100 ms) rather than advancing, so a lost packet is never skipped.
  Only when the host never responds to the header at all does the device
  fall back to open-loop streaming with a fixed inter-packet delay, as the
  SDS spec allows.
- If the requested slot has no sample, the device replies with an SDS
  CANCEL (`F0 7E 65 7D 00 F7`).
- The device plays back at 44100 Hz only, so outgoing dump headers always
  report 44100 Hz.
- A download cannot start while an upload is in progress and vice versa;
  the conflicting request is refused (CANCEL/NAK).

```bash
tools/drumtool/drumtool.js receive 0 kick.wav    # slot 0 as 44.1kHz WAV
tools/drumtool/drumtool.js receive 30 --raw      # slot 30 as raw 16-bit PCM
```

### Firmware Update Protocol

Firmware can be updated over SysEx without entering the bootloader. The device
streams the standard `.uf2` build artifact into the inactive A/B partition and
uses the RP2350 bootrom's try-before-you-buy (TBYB) mechanism, so a failed or
broken update can never brick the device. `tools/drumtool/drumtool.js flash
firmware.uf2` is the reference client.

1.  **Begin Update:**
    - **Command:** `BeginFirmwareUpdate` (0x20)
    - **Payload (encoded like BeginFileWrite):** total UF2 size (32-bit LE),
      SHA-256 of the UF2 stream (32 bytes), version major/minor/patch (3 bytes,
      informational).
    - The device resolves the inactive partition and replies `Ack`/`Nack`. The
      request is refused while a previous update is still in its unbought trial
      boot.

2.  **Send Data Chunks:**
    - **Command:** `FirmwareBytes` (0x21)
    - **Payload:** raw UF2 stream bytes in the same 7-bytes-plus-MSBs encoding
      as `FileBytes`. The device parses UF2 blocks (skipping the RP2350-A2
      erratum "absolute" block), rebases addresses to the inactive partition
      and programs flash one 4 KiB sector at a time. The `Ack` is sent only
      after the data has been written, so the host self-throttles.

3.  **End Update:**
    - **Command:** `EndFirmwareUpdate` (0x22)
    - The device verifies completeness and the SHA-256, replies `Ack`, then
      reboots into the new image in trial mode. The new firmware commits itself
      (`rom_explicit_buy`) after ~5 seconds of healthy operation; if it crashes
      or hangs before that, the watchdog reboot falls back to the previous
      firmware.
    - `AbortFirmwareUpdate` (0x23) or a 5-second inactivity timeout cancels an
      update in progress; only the inactive partition is affected.

Notes:
- Firmware images are built with the TBYB flag, so picotool/BOOTSEL loads also
  self-commit on first boot via the same mechanism.
- Both RAM (`PICO_COPY_TO_RAM`, the default) and flash (XIP) builds are
  relocatable between partitions and safe to distribute for SysEx updates.
  Images are linked to the XIP window base (0x10000000), and the RP2350
  bootrom programs QMI address translation on every partition boot so the
  booted partition appears at that address; partition A is not at flash
  offset 0, so even A-boots rely on this. Flash reads that must bypass the
  translation (e.g. the data partition) use storage offsets or the
  untranslated XIP alias. Verified on hardware in both directions (A->B and
  B->A) with an XIP build.
- If both partitions ever hold invalid images, the bootrom falls back to the
  USB (UF2) bootloader; LED indication is not possible in that mode.

### Sequencer State Commands
These commands allow reading and writing the current sequencer pattern state, including step velocities and active note assignments for each track.

#### RequestSequencerState (0x30)
Requests the current sequencer state from the device. No payload.

**Response:** The device replies with a `SequencerStateResponse` message containing the current pattern.

#### SequencerStateResponse (0x31)
The device's response to `RequestSequencerState`, containing the full sequencer state.

**Response Format:**
```
F0 00 22 01 65 31 <Payload> F7
```

**Payload (36 bytes):**
- Bytes 0-31: Velocity data for all steps (4 tracks x 8 steps)
  - Track 0 steps (0-7): bytes 0-7
  - Track 1 steps (0-7): bytes 8-15
  - Track 2 steps (0-7): bytes 16-23
  - Track 3 steps (0-7): bytes 24-31
  - Velocity 0 = step disabled, 1-127 = step enabled with velocity
- Bytes 32-35: Active MIDI note per track
  - Track 0 note: byte 32
  - Track 1 note: byte 33
  - Track 2 note: byte 34
  - Track 3 note: byte 35

All values are 7-bit MIDI-compliant (0-127).

#### SetSequencerState (0x32)
Sets the sequencer state on the device.

**Command Format:**
```
F0 00 22 01 65 32 <Payload> F7
```

**Payload:** Same 36-byte structure as `SequencerStateResponse` (see above), sent as raw 7-bit bytes directly after the command byte (no 3-to-16bit packing, unlike `BeginFileWrite`).

**Response:** The device replies with `Ack` (0x13) on success, or `Nack` (0x14) if the payload is malformed or too short.

**Notes:**
- If the payload layout ever changes, a new SysEx tag should be introduced for the new format so existing tools keep working.
- Setting sequencer state marks the internal storage as dirty, triggering an automatic save to flash after a debounce period.
- Sample selection is done by sending MIDI note numbers matching the desired sample slots (as documented in the MIDI Note Numbers section above).

### Settings Commands

Generic key-value access to device settings. Each setting has a 7-bit id, a
valid range, and a compile-time default (see `drum/settings.h`). Values are
persisted on the device as one file per setting under `/settings/`; a missing
or invalid file means the default applies.

| Setting | ID | Range | Default | Description |
|---------|-----|-------|---------|-------------|
| `midi_channel` | 0x01 | 1-16 | 10 | MIDI channel for incoming and outgoing notes and CCs |
| `slider_mode` | 0x02 | 0-7 | 1 | Bit mask of what the track slider controls: bit 0 = pitch, bit 1 = gain, bit 2 = decay. Decay ramps gain linearly from full at the trigger to silence at the slider-set fraction of the sample's playback duration (slider at max = no fade) |

#### GetSetting (0x40)
Requests the current value of one setting.

**Command Format:**
```
F0 00 22 01 65 40 <setting id> F7
```

**Response:** A `SettingValue` message, or `Nack` (0x14) for unknown setting ids.

#### SettingValue (0x41)
The device's response to `GetSetting`.

**Response Format:**
```
F0 00 22 01 65 41 <setting id> <value> F7
```

#### SetSetting (0x42)
Sets and persists one setting. The new value takes effect immediately.

**Command Format:**
```
F0 00 22 01 65 42 <setting id> <value> F7
```

**Response:** `Ack` (0x13) on success, or `Nack` (0x14) for unknown setting
ids, out-of-range values, or persistence failures.
