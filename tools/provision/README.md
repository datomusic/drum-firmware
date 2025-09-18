# Provisioning Tool

TypeScript rewrite of the original `tools/provision.sh`. Automates the manufacturing flow for a blank RP2350 by white-labeling OTP, installing partitions, flashing firmware, formatting storage, and loading the factory sample set.

## Prerequisites
- macOS host
- `picotool` and `node` in `$PATH`
- Device connected in BOOTSEL mode when the tool starts
- Firmware UF2 at either `tools/provision/drum.uf2` or `drum/build/drum.uf2`
- Partition UF2 at `tools/provision/partition_table.uf2` (optional, will be generated from `drum/partition_table.json` if missing)
- Factory samples in `support/samples/factory_kit`

## Building
Compile the script once (output stays in `tools/provision/dist/`):

```bash
npx tsc --module commonjs --target ES2020 --outDir tools/provision/dist tools/provision/index.ts
```

## Running
Invoke the compiled CLI from the repository root:

```bash
node tools/provision/dist/index.js
```

The tool mirrors the legacy bash script, prompting or aborting on the same error conditions. It exits non-zero if any provisioning step fails.

### JSON Logging
Pass `--json` to stream newline-delimited JSON events, which is useful when embedding in web services:

```bash
node tools/provision/dist/index.js --json
```

Each log line has the shape `{"type":"log","level":"info","message":"...","timestamp":"..."}`. A final summary entry carries the overall success flag.

## Artifact Locations
- Place production-ready firmware at `tools/provision/drum.uf2` to keep manufacturing assets co-located.
- `tools/provision/partition_table.uf2` is reused between runs; delete it to force regeneration from the JSON schema.

## Notes
- USB detection relies on macOS `system_profiler`, matching the behaviour of the original shell script.
- The tool reboots the device several times; expect the MIDI port to disappear and reappear during execution.
