# Musin Project

Musin is a collection of C++ libraries and drivers designed to facilitate the creation of embedded musical instruments and audio applications, primarily targeting the Raspberry Pi Pico (RP2040) platform. It integrates parts of the Pico SDK, external libraries like ETL (Embedded Template Library), and custom drivers/components.

## Project Goals

*   Provide reusable components for common hardware (audio codecs, LEDs, buttons, analog inputs).
*   Offer building blocks for audio processing (mixing, effects, sample playback).
*   Simplify integration with standard protocols like USB MIDI.
*   Enable rapid prototyping and development of firmware for custom musical hardware.

## Structure and Build System (CMake)

Musin uses CMake as its build system and is structured as a collection of modular libraries. This allows applications using Musin to pick and choose only the components they need, improving build times and reducing final binary size.

Instead of using monolithic initialization macros, Musin defines CMake targets (mostly `INTERFACE` or `STATIC` libraries) for each logical component.

### Core Components (CMake Targets)

The main components are defined as CMake libraries, typically within the `musin/CMakeLists.txt` file:

*   `musin::core`: Basic utilities, potentially including UART wrappers (`pico_uart.cpp`), core data structures, and common dependencies like `pico_stdlib` and `etl::etl`. Often a `STATIC` library if it contains compiled code.
*   `musin::hal`: Hardware Abstraction Layer components, such as `AnalogIn`, multiplexer handling, etc. Links against relevant `hardware_*` libraries (e.g., `hardware_adc`, `hardware_gpio`). Usually an `INTERFACE` library linking a private `STATIC` library for its sources.
*   `musin::drivers`: Drivers for specific hardware components like WS2812 LEDs (`ws2812.cpp`), audio codecs (`aic3204.c`), etc. May involve PIO program generation. Often structured as `INTERFACE` libraries linking private `STATIC` libraries.
*   `musin::audio`: Audio processing blocks (Mixer, Crusher, Filter, PitchShifter, Waveshaper, SampleReader implementations) and audio output handling (`audio_output.cpp`). Links against `musin::core`, `musin::hal`, `pico_audio_i2s`, `hardware_*` libs, etc.
*   `musin::ui`: User interface elements like keypad drivers (`keypad_hc138.cpp`), potentially drumpad handling logic. Links against `hardware_gpio`, etc.
*   `musin::usb_midi`: USB MIDI functionality, wrapping TinyUSB and potentially MIDI library implementations. Links against `tinyusb_device`, `tinyusb_board`.
*   `musin::filesystem`: Wrappers or integration for filesystem access (e.g., LittleFS via pico-vfs).

*(Note: The exact structure and naming might evolve.)*

### Using Musin Components in an Application

An application (e.g., an experiment in the `experiments/` directory) would use Musin components like this in its `CMakeLists.txt`:

1.  **Include Musin:** Add the Musin directory to the build.
    ```cmake
    # In application's CMakeLists.txt
    add_subdirectory(path/to/musin musin_build)
    ```

2.  **Link Needed Components:** Link the application executable against the required Musin component libraries and any necessary Pico SDK hardware libraries not pulled in transitively.
    ```cmake
    target_link_libraries(my_application PRIVATE
        pico_stdlib # Base SDK library

        # Musin components:
        musin::core
        musin::hal
        musin::drivers # For LEDs
        musin::ui      # For Keypad
        # musin::audio   # If using audio features
        # musin::usb_midi # If using MIDI

        # Other direct dependencies if needed
        hardware_clocks
    )
    ```

CMake automatically handles linking the dependencies of the specified Musin components (e.g., linking `musin::audio` will also link `musin::core`, `musin::hal`, `pico_audio_i2s`, etc.).

### Benefits

*   **Modularity:** Clear separation of concerns.
*   **Maintainability:** Easier to manage dependencies within Musin and for applications.
*   **Reduced Coupling:** Applications only depend on the parts of Musin they actually use.
*   **Improved Build Times:** Only necessary components are compiled and linked.
