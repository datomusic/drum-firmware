# Drumtool - DRUM Device Management Tool

Comprehensive tool for managing DATO DRUM devices via MIDI, featuring sample transfer using the MIDI Sample Dump Standard (SDS) protocol and device management commands.

## Features

- 🎵 **Sample Transfer**: Upload PCM audio samples using reliable SDS protocol
- 📦 **Batch Operations**: Transfer multiple samples in a single command
- 🎯 **Flexible Slot Assignment**: Auto-increment or explicit slot targeting
- 🔧 **Device Management**: Firmware version, filesystem formatting, bootloader access
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
# Transfer files to slots 0, 1, 2 automatically
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

#### Device Management

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
- Slots: 0-127 (128 total slots available)

### File Format Requirements

- **Format**: 16-bit PCM audio files (.wav, .pcm)
- **Sample Rate**: Any rate supported by your audio files (tool will encode correctly)
- **Channels**: Mono preferred (stereo files will use left channel)

## Examples

### Getting Started
```bash
# Check if device is connected and get firmware version
node drumtool.js version

# Transfer your first sample to slot 0
node drumtool.js send my-kick.wav:0

# Transfer a complete drum kit (auto-assigns slots 0-3)
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

### Device Maintenance
```bash
# Check device status
node drumtool.js version

# Clean slate - format and upload new samples
node drumtool.js format
node drumtool.js send new-kit/*.wav

# Enter firmware update mode
node drumtool.js reboot-bootloader
```

## Protocol Details

Drumtool uses the **MIDI Sample Dump Standard (SDS)** protocol for reliable sample transfer:

- **Header packets**: Sample metadata (rate, length, loop points)
- **Data packets**: 40 samples per packet with checksum validation
- **Handshaking**: ACK/NAK responses with timeout fallback
- **Status monitoring**: Device busy/error state detection

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
- Check available storage space on device with `drumtool.js version`

### Permission Issues (macOS/Linux)
- Ensure user has access to MIDI devices
- May need to run with elevated permissions: `sudo node drumtool.js ...`

## Architecture

Drumtool features a modular, platform-agnostic architecture:

- **Core Logic**: Platform-independent business logic in `src/core/`
- **Protocol Modules**: SDS and custom protocol implementations in `src/protocol/`
- **Platform Abstraction**: Interface definitions in `src/interfaces/`
- **Node.js Implementation**: Platform-specific implementations in `src/nodejs/`
- **CLI Layer**: Command-line interface orchestration in `src/cli/`

This design enables:
- 🧪 **Comprehensive Testing**: Full test coverage without hardware dependencies
- 🌐 **Platform Portability**: Core logic reusable for web applications
- 🔧 **Easy Maintenance**: Clean separation of concerns
- 📈 **Extensibility**: Simple to add new features and platforms

## Development

### Testing
```bash
# Run protocol tests
node test/protocol_test.js

# Run file processor tests
node test/file_processor_test.js

# Run integration tests
node test/integration_test.js

# Manual testing scenarios
# See `drumtool_test.md` for comprehensive test procedures
```

### Contributing
- Follow existing code style (2-space indentation)
- Add tests for new features in the `test/` directory
- Use the modular architecture - separate concerns properly
- Update documentation for API changes

## Technical Notes

- **Slot Range**: 0-127 (128 total slots)
- **Sample Rate Encoding**: Automatic conversion to SDS nanosecond period format
- **File Size Limits**: Determined by device storage capacity
- **Concurrent Transfers**: Single-threaded for reliability
- **Error Recovery**: Automatic retry on NAK, graceful degradation to non-handshaking mode

## License

Part of the DATO DRUM firmware project.