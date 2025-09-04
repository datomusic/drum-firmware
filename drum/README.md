# DATO DRUM MIDI Implementation Chart

## General Settings
- **Default MIDI Channel:** 10 (GM Percussion Standard)
- **Note Off Messages:** Ignored
- **Velocity Range:** 1-127
- **Clock:** Master by default, slave when external clock detected

## MIDI Note Numbers (Sample Mapping)

### Track 1 - Kick/Bass Drums
| Sample # | MIDI Note | GM Percussion Name |
|----------|-----------|-------------------|
| 1 | 35 | Acoustic Bass Drum |
| 2 | 36 | Bass Drum 1 |
| 3 | 37 | Side Stick |
| 4 | 41 | Low Floor Tom |
| 5 | 43 | High Floor Tom |
| 6 | 47 | Low-Mid Tom |
| 7 | 48 | Hi-Mid Tom |
| 8 | 50 | High Tom |

### Track 2 - Snare Drums
| Sample # | MIDI Note | GM Percussion Name |
|----------|-----------|-------------------|
| 1 | 38 | Acoustic Snare |
| 2 | 40 | Electric Snare |
| 3 | 39 | Hand Clap |
| 4 | 54 | Tambourine |
| 5 | 56 | Cowbell |
| 6 | 75 | Claves |
| 7 | 76 | Hi Wood Block |
| 8 | 77 | Low Wood Block |

### Track 3 - Percussion
| Sample # | MIDI Note | GM Percussion Name |
|----------|-----------|-------------------|
| 1 | 45 | Low Tom |
| 2 | 58 | Vibraslap |
| 3 | 59 | Ride Cymbal 2 |
| 4 | 60 | Hi Bongo |
| 5 | 61 | Low Bongo |
| 6 | 62 | Mute Hi Conga |
| 7 | 63 | Open Hi Conga |
| 8 | 64 | Low Conga |

### Track 4 - Hi-Hats & Cymbals
| Sample # | MIDI Note | GM Percussion Name |
|----------|-----------|-------------------|
| 1 | 42 | Closed Hi-Hat |
| 2 | 44 | Pedal Hi-Hat |
| 3 | 46 | Open Hi-Hat |
| 4 | 49 | Crash Cymbal 1 |
| 5 | 51 | Ride Cymbal 1 |
| 6 | 52 | Chinese Cymbal |
| 7 | 53 | Ride Bell |
| 8 | 57 | Crash Cymbal 2 |

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
- All MIDI channels, CC numbers, and note mappings configurable via web interface
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

A utility tool, `tools/drumtool/drumtool.js`, is provided to handle device communication and sample management using MIDI Sample Dump Standard (SDS) protocol.

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
