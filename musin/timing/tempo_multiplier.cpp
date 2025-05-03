#include "musin/timing/tempo_multiplier.h"
#include "musin/timing/sequencer_tick_event.h"
#include "musin/timing/timing_constants.h"
#include <algorithm> // For std::clamp
#include <cmath>     // For std::max, std::floor

namespace Musin::Timing {

TempoMultiplier::TempoMultiplier(int initial_multiplier, int initial_divider)
    : _multiplier(std::max(1, initial_multiplier)), _divider(std::max(1, initial_divider)),
      _even_swing_amount(0.0f), _odd_swing_amount(0.0f), _input_ticks_per_output_tick(0),
      _input_tick_counter(0), _output_tick_counter(0) {
  update_ticks_per_output();
}

void TempoMultiplier::notification([[maybe_unused]] Musin::Timing::TempoEvent event) {
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
    reset();
  }
}

void TempoMultiplier::set_divider(int divider) {
  int new_divider = std::max(1, divider);
  if (new_divider != _divider) {
    _divider = new_divider;
    update_ticks_per_output();
    reset();
  }
}

void TempoMultiplier::set_even_swing(float amount) {
  _even_swing_amount = std::clamp(amount, 0.0f, 0.999f); // Clamp below 1.0
}

void TempoMultiplier::set_odd_swing(float amount) {
  _odd_swing_amount = std::clamp(amount, 0.0f, 0.999f); // Clamp below 1.0
}

void TempoMultiplier::reset() {
  _input_tick_counter = 0;
  _output_tick_counter = 0;
}

void TempoMultiplier::update_ticks_per_output() {
  // Calculate how many high-resolution input ticks make up one output tick.
  // Base resolution is Clock::InternalClock::PPQN (e.g., 96)
  // Output resolution = Base * Multiplier / Divider
  // Ticks per output = Base / (Output Resolution / Base) = Base / (Multiplier / Divider)
  // Ticks per output = (Base / 2) * Divider / Multiplier  (Base is 8th notes)
  if (_multiplier > 0) {
    // Use floating point for intermediate calculation for better accuracy.
    // Base rate is 8th notes (DEFAULT_PPQN / 2.0)
    _input_ticks_per_output_tick = static_cast<uint32_t>(
        std::round((static_cast<double>(Musin::Timing::DEFAULT_PPQN) / 2.0) *
                   static_cast<double>(_divider) / static_cast<double>(_multiplier)));
  } else {
    // Avoid division by zero, default to 8th note base
    _input_ticks_per_output_tick = Musin::Timing::DEFAULT_PPQN / 2;
  }
  // Ensure minimum of 1 tick
  _input_ticks_per_output_tick = std::max(static_cast<uint32_t>(1u), _input_ticks_per_output_tick);
}

} // namespace Musin::Timing
