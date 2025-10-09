# Drumtool - DRUM Device Management Tool

Comprehensive tool for managing DATO DRUM devices via MIDI, featuring sample transfer using the MIDI Sample Dump Standard (SDS) protocol and device management commands.

## Features

- 🎵 **Sample Transfer**: Upload PCM audio samples using reliable SDS protocol
- 📦 **Batch Operations**: Transfer multiple samples in a single command
- 🎯 **Flexible Slot Assignment**: Auto-increment or explicit slot targeting
- 🔧 **Device Management**: Firmware version, filesystem formatting, bootloader access
- 💾 **Firmware Updates**: Safe over-the-air firmware updates via MIDI with UF2 validation
- 📊 **Progress Monitoring**: Real-time transfer progress with device status feedback
- ✅ **Safe Operations**: Confirmation prompts for destructive commands

## Installation

### Prerequisites

- Node.js (version 14 or higher)
- Connected DATO DRUM device via USB/MIDI

### Setup

```bash
cd tools/drumtool
npm install
```

## Usage

### Basic Syntax

```bash
node drumtool.js <command> [options]
```

### Commands

#### Sample Transfer

**Auto-increment slots (recommended for new users):**
```bash
# Transfer files to slots 30, 31, 32 automatically
node drumtool.js send kick.wav snare.wav hat.wav

# With custom sample rate
node drumtool.js send kick.wav snare.wav 48000

# Verbose output for debugging
node drumtool.js send kick.wav snare.wav -v
```

**Explicit slot assignment:**
```bash
# Transfer to specific slots
node drumtool.js send kick.wav:5 snare.wav:10

# Mixed with custom sample rate
node drumtool.js send kick.wav:0 snare.wav:1 48000 --verbose
```

#### Firmware Updates

**Update device firmware (⚠️ DESTRUCTIVE):**
```bash
node drumtool.js flash firmware.uf2
```

**With verbose output:**
```bash
node drumtool.js flash firmware.uf2 --verbose
```

**Important firmware update notes:**
- ⚠️ Device will reboot automatically after successful update
- Firmware files must be in UF2 format for RP2350 family
- Invalid or corrupted firmware will be rejected before installation
- Transfer is validated with CRC32 checksum
- Progress is shown during transfer
- Confirmation required before starting

### Device Management

**Check firmware version:**
```bash
node drumtool.js version
```

**Format filesystem (⚠️ DESTRUCTIVE):**
```bash
node drumtool.js format
```

**Reboot to bootloader mode:**
```bash
node drumtool.js reboot-bootloader
```

### Options

- `--verbose`, `-v` - Show detailed transfer information and debugging output
- Sample rates: Specify as number (e.g., `44100`, `48000`) - applies to all files in the command
- Slots: 0-127 (128 total slots available, auto-assignment starts at slot 30)

### File Format Requirements

- **Format**: 16-bit PCM audio files (.wav, .pcm)
- **Sample Rate**: Any rate supported by your audio files (tool will encode correctly)
- **Channels**: Mono preferred (stereo files will use left channel)

## Examples

### Getting Started
```bash
# Check if device is connected and get firmware version
node drumtool.js version

# Transfer your first sample to slot 30
node drumtool.js send my-kick.wav:30

# Transfer a complete drum kit (auto-assigns slots 30-33)
node drumtool.js send kick.wav snare.wav hat-closed.wav hat-open.wav
```

### Advanced Usage
```bash
# High-quality samples with custom sample rate
node drumtool.js send kick.wav snare.wav 48000 -v

# Precise slot targeting for organized kits
node drumtool.js send kick.wav:35 snare.wav:38 hat-closed.wav:42

# Batch transfer with progress monitoring
node drumtool.js send samples/*.wav --verbose
```

### Firmware Updates
```bash
# Check current firmware version before updating
node drumtool.js version

# Update firmware (with confirmation prompt)
node drumtool.js flash /path/to/firmware.uf2

# Update with detailed progress information
node drumtool.js flash /path/to/firmware.uf2 --verbose

# Device will reboot automatically after successful update
# Check new version after reboot
node drumtool.js version
```

### Device Maintenance
```bash
# Check device status
node drumtool.js version

# Clean slate - format and upload new samples
node drumtool.js format
node drumtool.js send new-kit/*.wav

# Enter firmware update mode (for USB mass storage updates)
node drumtool.js reboot-bootloader
```

## Protocol Details

### Sample Transfer Protocol

Drumtool uses the **MIDI Sample Dump Standard (SDS)** protocol for reliable sample transfer:

- **Header packets**: Sample metadata (rate, length, loop points)
- **Data packets**: 40 samples per packet with checksum validation
- **Handshaking**: ACK/NAK responses with timeout fallback
- **Status monitoring**: Device busy/error state detection

### Firmware Transfer Protocol

Firmware updates use a custom SysEx protocol with UF2 validation:

- **UF2 Format**: Industry-standard firmware format with 512-byte blocks
- **Family Validation**: Ensures firmware is for RP2350 ARM-S devices
- **Binary Encoding**: 7-to-8 encoding for MIDI-safe transmission (7 data bytes + 1 MSB byte)
- **CRC32 Checksum**: Full-file integrity verification
- **Block Validation**: Magic numbers, block sequencing, duplicate detection
- **Transfer Flow**:
  1. BeginFirmwareWrite with path, size, and CRC32
  2. FileBytes messages with encoded UF2 blocks (512 bytes per chunk)
  3. EndFileTransfer with device-side verification
  4. Automatic device reboot on success

## Troubleshooting

### Device Not Found
```
No suitable MIDI output port found containing any of: Pico, DRUM
```
**Solution**: 
- Check USB cable connection
- Verify device is powered on
- Close other MIDI applications that might be using the device

### Transfer Failures
- Use `--verbose` flag to see detailed error information
- Try smaller batch sizes if transfers fail consistently
- Check available storage space on device with `drumtool.js storage`

### Firmware Update Issues

**Invalid UF2 file:**
```
Error: Invalid UF2 file: Wrong family ID (expected RP2350 0xe48bff59, got 0x...)
```
**Solution**: Ensure you're using firmware built for RP2350 devices (not RP2040 or other platforms)

**Transfer interrupted:**
- If firmware transfer fails partway through, the device remains on the old firmware
- Simply retry the flash command - the device is safe to retry
- Use `--verbose` to see exactly where the transfer failed

**Device not rebooting after update:**
- Wait 10-15 seconds after "Device will reboot automatically" message
- Manually power cycle the device if it doesn't reboot
- Check firmware version with `drumtool.js version` after reboot

**Verification failed:**
```
Error: Device firmware verification failed
```
**Solution**:
- Check that all UF2 blocks were received (use `--verbose`)
- Verify the UF2 file isn't corrupted (try re-downloading)
- Ensure sufficient storage space on device
- Try the transfer again

### Permission Issues (macOS/Linux)
- Ensure user has access to MIDI devices
- May need to run with elevated permissions: `sudo node drumtool.js ...`

## Development

### Testing
See `drumtool_test.md` for comprehensive test scenarios and expected outputs.

### Contributing
- Follow existing code style (2-space indentation)
- Add tests for new features
- Update documentation for API changes

## Technical Notes

- **Slot Range**: 0-127 (128 total slots, auto-assignment starts at slot 30 for linear MIDI mapping)
- **MIDI Mapping**: Slot N corresponds to MIDI note N (e.g., slot 36 = MIDI note 36)
- **Sample Rate Encoding**: Automatic conversion to SDS nanosecond period format
- **File Size Limits**: Determined by device storage capacity
- **Concurrent Transfers**: Single-threaded for reliability
- **Error Recovery**: Automatic retry on NAK, graceful degradation to non-handshaking mode

## License

Part of the DATO DRUM firmware project.