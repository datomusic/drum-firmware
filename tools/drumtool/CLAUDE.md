# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Drumtool is a Node.js CLI for managing DATO DRUM devices via MIDI Sample Dump Standard (SDS). See `README.md` for usage details.

## Development Commands

```bash
npm install
node drumtool.js version                    # Test device connection
node drumtool.js send file.wav:0 --verbose  # Upload sample with debugging
```

## Architecture

- **drumtool.js**: Main CLI with MIDI connection management and command routing  
- **sds_protocol.js**: Pure SDS protocol functions extracted for testing
- **Key pattern**: All argument validation happens before MIDI initialization

## Testing

See `drumtool_test.md` for comprehensive test scenarios. Test invalid arguments first (should exit without MIDI init), then device communication.