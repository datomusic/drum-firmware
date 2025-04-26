#include "tempo_multiplier.h"
#include <algorithm> // For std::clamp
#include <cmath>     // For std::max, std::floor
#include <cstdio>    // For printf

namespace Tempo {

TempoMultiplier::TempoMultiplier(int initial_multiplier, int initial_divider)
    : _multiplier(std::max(1, initial_multiplier)),
      _divider(std::max(1, initial_divider)), _even_swing_amount(0.0f), _odd_swing_amount(0.0f),
      _input_ticks_per_output_tick(0), _input_tick_counter(0), _output_tick_counter(0) {
  update_ticks_per_output();
  printf("TempoMultiplier: Initialized. Multiplier: %d, Divider: %d, Ticks/Output: %lu\n",
         _multiplier, _divider, _input_ticks_per_output_tick);
}

void TempoMultiplier::notification(const Tempo::TempoEvent &event) {
  _input_tick_counter++;

  // Calculate the ideal trigger point for the *current* output tick (relative to start of pattern)
  // Note: _output_tick_counter represents the *last completed* output tick.
  // We are checking if we have reached the point to emit the *next* one.
  uint32_t ideal_trigger_point_for_next_tick = (_output_tick_counter + 1) * _input_ticks_per_output_tick;

  // Determine if the *next* output tick is odd or even (1-based index)
  bool is_next_output_odd = ((_output_tick_counter + 1) % 2) != 0;

  // Calculate swing delay in input ticks
  float swing_amount = is_next_output_odd ? _odd_swing_amount : _even_swing_amount;
  // Delay is swing_amount * interval between output ticks. Clamped swing amount ensures delay < interval.
  uint32_t swing_delay_ticks =
      static_cast<uint32_t>(swing_amount * static_cast<float>(_input_ticks_per_output_tick));

  // Calculate the actual trigger point including swing delay
  uint32_t actual_trigger_point_for_next_tick = ideal_trigger_point_for_next_tick + swing_delay_ticks;

  // Check if the current input tick count has reached or passed the trigger point
  // We use >= to handle cases where the timer callback might be slightly delayed.
  if (_input_tick_counter >= actual_trigger_point_for_next_tick) {
    // Emit the sequencer tick
    SequencerTickEvent tick_event;
    // TODO: Populate event with step index or timestamp if needed
    // tick_event.step_index = _output_tick_counter;
    etl::observable<etl::observer<SequencerTickEvent>, MAX_SEQUENCER_OBSERVERS>::notify_observers(
        tick_event);

    // Increment output counter *after* sending notification
    _output_tick_counter++;

    // --- Resetting Input Counter ---
    // To prevent drift, especially with swing, we should reset the input counter
    // relative to the *ideal* trigger point of the tick we just emitted.
    // The ideal point was: _output_tick_counter * _input_ticks_per_output_tick (using the *new* value)
    uint32_t ideal_trigger_point_emitted_tick = _output_tick_counter * _input_ticks_per_output_tick;
    _input_tick_counter -= ideal_trigger_point_emitted_tick;

    // If swing caused the actual trigger point to be later than the ideal point
    // of the *next* tick (unlikely but possible with extreme settings/timing issues),
    // ensure the counter doesn't go negative or wrap unexpectedly.
    // This simple subtraction assumes _input_tick_counter was >= ideal_trigger_point_emitted_tick.
    // A more robust approach might involve absolute timestamps if available.

    // printf("Tick Out: %lu (Input Reset To: %lu, Ideal Emit Point: %lu, Actual Emit Point: %lu, Swing Delay: %lu)\n",
    //        _output_tick_counter, _input_tick_counter, ideal_trigger_point_emitted_tick,
    //        actual_trigger_point_for_next_tick, swing_delay_ticks);
  }
}


void TempoMultiplier::set_multiplier(int multiplier) {
  int new_multiplier = std::max(1, multiplier);
  if (new_multiplier != _multiplier) {
    _multiplier = new_multiplier;
    update_ticks_per_output();
    printf("TempoMultiplier: Multiplier set to %d. Ticks/Output: %lu\n", _multiplier,
           _input_ticks_per_output_tick);
    // Reset counters when multiplier/divider changes to avoid strange timing jumps
    reset();
  }
}

void TempoMultiplier::set_divider(int divider) {
  int new_divider = std::max(1, divider);
  if (new_divider != _divider) {
    _divider = new_divider;
    update_ticks_per_output();
    printf("TempoMultiplier: Divider set to %d. Ticks/Output: %lu\n", _divider,
           _input_ticks_per_output_tick);
    // Reset counters when multiplier/divider changes
    reset();
  }
}

void TempoMultiplier::set_even_swing(float amount) {
  _even_swing_amount = std::clamp(amount, 0.0f, 0.999f); // Clamp below 1.0
  // printf("TempoMultiplier: Even swing set to %.3f\n", _even_swing_amount);
}

void TempoMultiplier::set_odd_swing(float amount) {
  _odd_swing_amount = std::clamp(amount, 0.0f, 0.999f); // Clamp below 1.0
  // printf("TempoMultiplier: Odd swing set to %.3f\n", _odd_swing_amount);
}

void TempoMultiplier::reset() {
  _input_tick_counter = 0;
  _output_tick_counter = 0;
  printf("TempoMultiplier: Counters reset.\n");
}

void TempoMultiplier::update_ticks_per_output() {
  // Calculate how many high-resolution input ticks make up one output tick.
  // Base resolution is Clock::InternalClock::PPQN (e.g., 96)
  // Output resolution = Base * Multiplier / Divider
  // Ticks per output = Base / (Output Resolution / Base) = Base / (Multiplier / Divider)
  // Ticks per output = Base * Divider / Multiplier
  if (_multiplier > 0) {
    // Use floating point for intermediate calculation for better accuracy
    _input_ticks_per_output_tick =
        static_cast<uint32_t>(std::round(static_cast<double>(Clock::InternalClock::PPQN) *
                                         static_cast<double>(_divider) / static_cast<double>(_multiplier)));
  } else {
    _input_ticks_per_output_tick = Clock::InternalClock::PPQN; // Avoid division by zero, default to base
  }
  // Ensure minimum of 1 tick
  _input_ticks_per_output_tick = std::max(static_cast<uint32_t>(1u), _input_ticks_per_output_tick);
}

} // namespace Tempo
