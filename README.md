# DRUM Firmware Repository

This repository contains the firmware and related code for the DRUM project.

## Directory Structure:

- `musin/`: Core C++ firmware source code.
  - `hal/`: Hardware Abstraction Layer for RP2350 peripherals.
  - `audio/`: Audio processing components.
  - `ui/`: User interface elements.
  - `usb/`: USB communication handling.
  - `ports/`: Platform-specific code and libraries (e.g., Pico SDK integration).
- `prototype/`: Python-based prototyping and testing code (likely for CircuitPython/MicroPython).
- `experiments/`: Standalone code experiments and tests.
- `tools/`: Utility scripts and tools for development/deployment.
- `lib/`: External libraries (may be deprecated or integrated elsewhere).
