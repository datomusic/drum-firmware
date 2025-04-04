# DRUM Firmware Repository

This repository contains the firmware and related code for the DRUM project.

## Directory Structure:
- `drum/`: Application C++ source code
- `musin/`: Core C++ library source code.
  - `drivers/`: Hardware device drivers (e.g., codecs, sensors).
  - `hal/`: Hardware Abstraction Layer for RP2350 peripherals.
  - `audio/`: Audio processing components.
  - `ui/`: User interface elements.
  - `usb/`: USB communication handling.
  - `ports/`: Platform-specific code and libraries (e.g., Pico SDK integration).
- `prototype/`: CircuitPython-based prototyping and testing code.
- `experiments/`: Standalone code experiments and tests.
- `tools/`: Utility scripts and tools for development/deployment.
- `lib/`: External libraries (may be deprecated or integrated elsewhere).
