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
- `test/`: Unit and integration tests for `musin` and `drum` code.
- `tools/`: Utility scripts and tools for development/deployment.
- `lib/`: External libraries. 

## Building the Firmware

The main firmware for the DRUM project is located in the `drum/` directory. To build the firmware, navigate to the `drum/` directory and run the following commands for a standard release build:

```bash
cd drum && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

For a development build with verbose logging enabled (which will output detailed logs over the serial USB connection), use the `ENABLE_VERBOSE_LOGGING` option and set the build type to `Debug`:

```bash
cd drum && cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_VERBOSE_LOGGING=ON && cmake --build build
```

## Running the Tests

```bash
cd test && ./run_all_tests.sh
```

### Sender Application Tests

The `test/sender` directory contains a Node.js/TypeScript test suite for verifying the device's MIDI communication protocols from a host computer.

To run these tests, first install the dependencies:

```bash
cd test/sender
npm install
```

Then, run the main test suite:

```bash
npm test
```

#### Optional and Destructive Tests

The test suite includes tests that are skipped by default because they are long-running, destructive, or require manual intervention. You can enable them using environment variables:

-   **Run long-running tests** (e.g., transferring large files):
    ```bash
    RUN_LARGE_TESTS=true npm test
    ```

-   **Run destructive tests** (e.g., formatting the filesystem). **Warning:** This will erase the device's storage.
    ```bash
    RUN_DESTRUCTIVE_TESTS=true npm test
    ```

-   **Run the reboot test**. This test verifies that the device can reboot into the bootloader and then automatically reboots it back to the main application using `picotool`.
    ```bash
    RUN_REBOOT_TESTS=true npm test
    ```
--- End of content ---
