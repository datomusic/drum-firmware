# DRUM Firmware Repository

This repository contains the firmware and related code for the DRUM project.

## Directory Structure:
- `drum/`: Application C++ source code
- `musin/`: Core C++ library source code.
  - `drivers/`: Hardware device drivers (e.g., codecs, sensors).
  - `hal/`: Hardware Abstraction Layer for microcontroller peripherals (e.g., RP2040, RP2350).
  - `audio/`: Audio processing components.
  - `ui/`: User interface elements.
  - `usb/`: USB communication handling.
  - `ports/`: Platform-specific code and libraries (e.g., Pico SDK integration).
- `prototype/`: CircuitPython-based prototyping and testing code.
- `experiments/`: Standalone code experiments and tests.
- `test/`: Unit and integration tests for `musin` and `drum` code.
- `tools/`: Utility scripts and tools for development/deployment.
- `lib/`: External libraries (may be deprecated or integrated elsewhere).
