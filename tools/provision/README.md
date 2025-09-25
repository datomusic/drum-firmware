# Provisioning Tool

TypeScript tool for automating the manufacturing flow of blank RP2350 devices. Handles white-labeling OTP, installing partitions, flashing firmware, formatting storage, and loading the factory sample set.

## Prerequisites

### System Requirements
- **Cross-platform** (macOS, Linux, Windows experimental)
- **Node.js 18+** and **npm** in `$PATH`
- **picotool** in `$PATH` for RP2350 operations
- **TypeScript** for building (installed via npm)

### Hardware Setup
- RP2350 device connected in **BOOTSEL mode** when starting
- Device should be blank/unprovisioned

### Required Files
- Firmware UF2: `tools/provision/drum.uf2` or `drum/build/drum.uf2`
- Partition table: `tools/provision/partition_table.uf2` (auto-generated if missing)
- Factory samples: `support/samples/factory_kit/` directory

## Building

Install dependencies and build:

```bash
cd tools/provision
npm install
npm run build
```

This compiles `index.ts` to `dist/index.js` using the project's TypeScript configuration.

### Build Commands
- `npm run build` - Compile TypeScript to JavaScript
- `npm run build:watch` - Watch mode for development
- `npm run clean` - Remove compiled output
- `npm run start` - Build and run in one command

## Running

**Important**: Run from the repository root directory, not from `tools/provision/`.

```bash
# From project root
node tools/provision/dist/index.js
```

### JSON Logging Mode
For automation or web service integration:

```bash
node tools/provision/dist/index.js --json
```

Outputs newline-delimited JSON with structured logging and a final summary.

## What It Does

1. **White-label bootloader** - Programs OTP memory (irreversible)
2. **Partition device** - Creates A/B firmware partitions
3. **Flash firmware** - Loads main application
4. **Format filesystem** - Prepares internal storage
5. **Upload samples** - Loads factory drum kit
6. **Verify** - Checks firmware version

The process takes 2-3 minutes and reboots the device several times. Exit codes indicate success (0) or failure (non-zero).

## Troubleshooting

- **Device not found**: Ensure device is in BOOTSEL mode (hold BOOTSEL while connecting)
- **Permission errors**: Check that `picotool` has proper USB access
- **Build failures**: Verify Node.js 18+ and TypeScript are installed
- **Missing files**: Check that firmware UF2 and sample files exist
- **Windows**: Volume detection may fail; tool will warn but continue operation
