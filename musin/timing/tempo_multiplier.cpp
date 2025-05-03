#include "musin/timing/tempo_multiplier.h"
#include "musin/timing/sequencer_tick_event.h"
// timing_constants.h and algorithm are now included via the header for constexpr
#include <algorithm> // For std::max used in setters

namespace Musin::Timing {

constexpr TempoMultiplier::TempoMultiplier(uint32_t initial_multiplier, uint32_t initial_divider)
    : _multiplier(std::max(1u, initial_multiplier)), _divider(std::max(1u, initial_divider)),
      _input_ticks_per_output_tick(0), _input_tick_counter(0), _output_tick_counter(0) {
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

void TempoMultiplier::set_multiplier(uint32_t multiplier) {
  uint32_t new_multiplier = std::max(1u, multiplier);
  if (new_multiplier != _multiplier) {
    _multiplier = new_multiplier;
    update_ticks_per_output();
    reset();
  }
}

void TempoMultiplier::set_divider(uint32_t divider) {
  uint32_t new_divider = std::max(1u, divider);
  if (new_divider != _divider) {
    _divider = new_divider;
    update_ticks_per_output();
    reset();
  }
}

void TempoMultiplier::reset() {
  _input_tick_counter = 0;
  _output_tick_counter = 0;
}

constexpr void TempoMultiplier::update_ticks_per_output() {
  // Calculate how many high-resolution input ticks make up one output tick using integer math.
  // Base resolution is DEFAULT_PPQN (e.g., 96 PPQN).
  // We assume the base tempo corresponds to 8th notes, so the base tick rate is (DEFAULT_PPQN / 2).
  // Formula: Ticks per output = (Base Rate * Divider) / Multiplier
  // Use uint64_t for intermediate calculation to prevent overflow.
  // Add (multiplier / 2) for rounding before integer division.
  constexpr uint64_t base_rate = static_cast<uint64_t>(Musin::Timing::DEFAULT_PPQN) / 2;
  uint64_t numerator = base_rate * static_cast<uint64_t>(_divider);
  uint64_t denominator = static_cast<uint64_t>(_multiplier); // Already ensured >= 1

  // Apply rounding before division
  uint64_t rounded_ticks = (numerator + (denominator / 2)) / denominator;

  // Store as uint32_t and ensure minimum of 1 tick.
  _input_ticks_per_output_tick =
      std::max(1u, static_cast<uint32_t>(rounded_ticks)); // Clamp result to uint32_t max if needed
}

} // namespace Musin::Timing
