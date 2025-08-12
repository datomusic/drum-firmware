# Plan: Main Loop State Machine

Our main `while(true)` loop is inefficient. It tries to do everything all the time, which slows down critical operations like file transfers. We're also burning CPU cycles by spinning without a `sleep` call, which is not ideal for power consumption or thermal management.

This plan introduces a high-level state machine to manage the application's operational mode, ensuring we only run what's necessary for the current task.

## The Plan

-   [ ] **1. Define Application States:**
    -   Create an `enum class` with at least two states:
        -   `SequencerMode`: The default mode for making music. All components are active. The main loop will be throttled with `sleep_us()` to conserve power.
        -   `FileTransferMode`: A high-performance mode activated during SysEx transfers. Only essential USB and file-handling components will be active. The main loop will run at maximum speed without sleeping.

-   [ ] **2. Create an `ApplicationGovernor` Class:**
    -   Create a new class (`drum/application_governor.h` and `.cpp`).
    -   This class will hold the current application state (`SequencerMode` by default).
    -   It will hold references to all the major components currently managed by `main.cpp` (e.g., `sequencer_controller`, `audio_engine`, `pizza_display`, `sysex_handler`, etc.).

-   [ ] **3. Implement State Transitions:**
    -   The `ApplicationGovernor` will be an `etl::observer` for `drum::Events::SysExTransferStateChangeEvent`.
    -   On receiving the event, it will change its internal state:
        -   `is_active: true` -> switch to `FileTransferMode`.
        -   `is_active: false` -> switch back to `SequencerMode`.
    -   This moves the responsibility for knowing *when* to change from `main` to a dedicated object reacting to system events.

-   [ ] **4. Refactor the Main Loop:**
    -   Instantiate the `ApplicationGovernor` in `main.cpp`.
    -   Register it as an observer for `sysex_handler`.
    -   The `main` `while(true)` loop will be simplified dramatically. It will primarily call a single method, e.g., `governor.tick()`.

-   [ ] **5. Implement Mode-Specific Logic in the Governor:**
    -   The `governor.tick()` method will contain a `switch` statement based on the current mode.
    -   `case SequencerMode`: Call `update()` on all musical components (`sequencer_controller`, `audio_engine`, `pizza_controls`, `pizza_display`, etc.) and end with a `sleep_us()` call.
    -   `case FileTransferMode`: Call `update()` *only* on the components required for the transfer (`sysex_handler`, `midi_manager`, `musin::usb::background_update`). Crucially, it will *not* call `sleep_us()`.

-   [ ] **6. Ensure Robustness (Elecia's Review):**
    -   Double-check that `SysExHandler` *always* fires a `SysExTransferStateChangeEvent(false)` on any termination condition (success, error, or timeout). This guarantees the device never gets stuck in `FileTransferMode`.
