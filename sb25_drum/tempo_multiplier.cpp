#include "tempo_multiplier.h"
#include <algorithm> // For std::clamp
#include <cmath>     // For std::max, std::floor
#include <cstdio>    // For printf

namespace Tempo {

TempoMultiplier::TempoMultiplier(int initial_multiplier, int initial_divider)
    : _multiplier(std::max(1, initial_multiplier)), _divider(std::max(1, initial_divider)),
      _even_swing_amount(0.0f), _odd_swing_amount(0.0f), _input_ticks_per_output_tick(0),
      _input_tick_counter(0), _output_tick_counter(0) {
  update_ticks_per_output();
  printf("TempoMultiplier: Initialized. Multiplier: %d, Divider: %d, Ticks/Output: %lu\n",
         _multiplier, _divider, _input_ticks_per_output_tick);
}

void TempoMultiplier::notification(Tempo::TempoEvent event) {
  _input_tick_counter++;

  if (_input_tick_counter >= _input_ticks_per_output_tick) {
    SequencerTickEvent tick_event;
    etl::observable<etl::observer<SequencerTickEvent>, MAX_SEQUENCER_OBSERVERS>::notify_observers(
        tick_event);

    _input_tick_counter -= _input_ticks_per_output_tick;
    _output_tick_counter++;
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
    _input_ticks_per_output_tick = static_cast<uint32_t>(
        std::round(static_cast<double>(Clock::InternalClock::PPQN) * static_cast<double>(_divider) /
                   static_cast<double>(_multiplier)));
  } else {
    _input_ticks_per_output_tick =
        Clock::InternalClock::PPQN; // Avoid division by zero, default to base
  }
  // Ensure minimum of 1 tick
  _input_ticks_per_output_tick = std::max(static_cast<uint32_t>(1u), _input_ticks_per_output_tick);
}

} // namespace Tempo
