# Refactoring the Clock System to a PLL Architecture

This document outlines the step-by-step plan to refactor the device's timing system from a source-switching model to a Phase-Locked Loop (PLL) architecture.

### Architectural Goal

The `InternalClock` will become the single, authoritative clock source for the entire system. It will run continuously. External signals (MIDI, SyncIn) will not *replace* it, but will instead *discipline* it by making small, continuous adjustments to its phase and frequency to keep it locked to the external reference.

---

### Implementation Checklist

#### Phase 1: Refactor `InternalClock` to be a Disciplined Oscillator

The `InternalClock` needs to be transformed from a simple timer into a self-regulating PLL.

-   [x] **1.1: Change `InternalClock` to run continuously.**
    -   In `internal_clock.cpp`, modify `start()` to begin a repeating timer that never stops on its own. The `stop()` method will be removed or repurposed. The clock should start running in its constructor or an `init()` method.
    -   The `timer_callback` must still return `bool` to match the Pico SDK's required signature, but our implementation will now always `return true;` to ensure it never stops.

-   [x] **1.2: Add PLL state variables to `InternalClock`.**
    -   In `internal_clock.h`, add private member variables to store the PLL state:
        ```cpp
        // The "master" tempo set by the user via UI
        float target_bpm_; 
        
        // The instantaneous, adjusted BPM after PLL correction
        float current_bpm_; 

        // PLL loop filter variables
        float phase_error_us_ = 0.0f;
        float phase_error_integral_ = 0.0f;

        // The time of the last generated internal tick
        absolute_time_t last_internal_tick_time_ = nil_time;

        // The source it should be listening to for corrections
        ClockSource discipline_source_ = ClockSource::INTERNAL;
        
        // PPQN of the current discipline source (e.g., 24 for MIDI)
        uint32_t discipline_ppqn_ = DEFAULT_PPQN; 
        ```

-   [x] **1.3: Create a new public method for receiving reference ticks.**
    -   In `internal_clock.h`, declare `void reference_tick_received(absolute_time_t now, ClockSource source);`.
    -   This method will be called by an external class (`TempoHandler`) when a MIDI or SyncIn tick arrives.

-   [x] **1.4: Implement the core PLL logic.**
    -   In `internal_clock.cpp`, implement `reference_tick_received()`. This is the heart of the PLL.
        1.  If `source != discipline_source_`, ignore the tick.
        2.  Calculate the expected interval of the reference clock based on our `current_bpm_` and its PPQN (e.g., 24 for MIDI).
        3.  Calculate the time we *expected* this tick to arrive based on the last reference tick.
        4.  `phase_error_us_ = time_of_this_tick - expected_time_of_this_tick;`
        5.  Update the integral term: `phase_error_integral_ += phase_error_us_;` (and clamp it to prevent wind-up).
        6.  Apply a PI controller formula to get a BPM adjustment: `float adjustment = (KP * phase_error_us_) + (KI * phase_error_integral_);` (`KP` and `KI` are tuning constants).
        7.  Update the clock's frequency: `current_bpm_ = target_bpm_ + adjustment;`.
        8.  Recalculate the `_tick_interval_us` for the hardware timer based on the new `current_bpm_`.

-   [x] **1.5: Modify the `timer_callback`.**
    -   The callback will now use the continuously updated `_tick_interval_us` to schedule its next execution.
    -   It will notify observers with a `ClockEvent{ClockSource::INTERNAL}`.

#### Phase 2: Simplify `TempoHandler`'s Role

`TempoHandler` no longer switches between clocks. It just tells `InternalClock` which reference to use.

-   [x] **2.1: Remove observer logic from `set_clock_source`.**
    -   In `tempo_handler.cpp`, the `set_clock_source` method should be gutted. It should no longer add/remove observers from `_internal_clock_ref`, `_midi_clock_processor_ref`, etc.
    -   Instead, it should simply call a new method on `_internal_clock_ref`, e.g., `_internal_clock_ref.set_discipline(source, ppqn_for_source);`.

-   [x] **2.2: Change `TempoHandler` to observe all potential sources.**
    -   In the `TempoHandler` constructor, it should *always* add itself as an observer to `_midi_clock_processor_ref` and `_sync_in_ref`.
    -   It should also observe the `_internal_clock_ref` to receive the final, disciplined ticks.

-   [x] **2.3: Rework the `notification` method.**
    -   In `tempo_handler.cpp`, the `notification` method (which receives a `ClockEvent`) will have two cases:
        1.  If the event is from MIDI or SyncIn, it calls `_internal_clock_ref.reference_tick_received(now, event.source);`. It does **not** forward this event.
        2.  If the event is from `InternalClock`, it means a final, disciplined tick has been generated. It then notifies its own observers with a `TempoEvent`, which contains no source information.

-   [x] **2.4: Update the `update` method.**
    -   The `update()` method in `TempoHandler` remains, but its logic changes slightly. It still detects if a cable is plugged in or if MIDI is active, but instead of calling `set_clock_source` to rewire observers, it calls it to tell the `InternalClock` which source to follow.

#### Phase 3: Integration and PPQN Handling

This phase connects everything and handles the different PPQN rates.

-   [x] **3.1: Define PPQN for each source.**
    -   MIDI is fixed at 24 PPQN.
    *   The `SyncIn` needs a configurable PPQN. This could be a new method `sync_in.set_ppqn(int ppqn)` that can be set from the UI or configuration. For now, you can hardcode it (e.g., to 4 for 16th notes).

-   [x] **3.2: Pass PPQN to the `InternalClock`.**
    -   When `TempoHandler` calls `_internal_clock_ref.set_discipline()`, it should pass both the source and the PPQN of that source. The `InternalClock` needs this to calculate the expected tick interval for its phase detector.

-   [x] **3.3: Update `main.cpp`.**
    -   The instantiation logic in `main.cpp` will likely not change much, but you need to verify that the observer relationships match the new design.
    -   `TempoHandler` observes `InternalClock`, `MidiClockProcessor`, and `SyncIn`.
    -   The `SequencerController` and `PizzaDisplay` *only* observe `TempoHandler`.

#### Phase 4: Tuning and Verification

-   [ ] **4.1: Expose `KP` and `KI` constants.**
    -   Make the Proportional (`KP`) and Integral (`KI`) gains easily changeable, perhaps as `constexpr` values at the top of `internal_clock.cpp`.

-   [ ] **4.2: Add Debug Output.**
    -   Inside the `InternalClock`, add `printf` statements (within `#ifdef VERBOSE`) to show the calculated `phase_error_us_` and the resulting `current_bpm_`. This is essential for tuning.

-   [ ] **4.3: Test and Tune.**
    -   **Test Case 1 (Lock-in):** Start with the internal clock at 120 BPM. Plug in a MIDI clock at 140 BPM. Watch the debug output to see the `current_bpm_` smoothly ramp up to 140.
    -   **Test Case 2 (Dropout):** While locked to MIDI at 140 BPM, unplug the cable. The `current_bpm_` should remain stable at 140.
    -   **Test Case 3 (Jitter):** Use a jittery MIDI source. The `current_bpm_` should remain relatively stable, with only minor fluctuations.
    -   **Test Case 4 (SyncIn):** Repeat the tests using the hardware Sync In.
