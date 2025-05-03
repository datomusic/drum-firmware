#ifndef SB25_DRUM_TEMPO_MULTIPLIER_H
#define SB25_DRUM_TEMPO_MULTIPLIER_H

#include "etl/observer.h"
#include "musin/timing/sequencer_tick_event.h"
#include "musin/timing/tempo_event.h"
#include <cstdint>

namespace Musin::Timing {

// Maximum number of observers TempoMultiplier can notify (e.g., SequencerController)
constexpr size_t MAX_SEQUENCER_OBSERVERS = 2;

/**
 * @brief Modifies tempo based on multiplier/divider settings and applies swing.
 *
 * Listens to TempoEvents (typically at a high resolution like 96 PPQN)
 * and emits SequencerTickEvents at a rate determined by the multiplier
 * and divider. It can also apply swing by delaying odd or even output ticks.
 */
class TempoMultiplier
    : public etl::observer<Musin::Timing::TempoEvent>,
      public etl::observable<etl::observer<Musin::Timing::SequencerTickEvent>, MAX_SEQUENCER_OBSERVERS> {
public:
  /**
   * @brief Constructor.
   * @param initial_multiplier Initial tempo multiplier.
   * @param initial_divider Initial tempo divider.
   */
  explicit TempoMultiplier(int initial_multiplier = 1, int initial_divider = 4);

  // Prevent copying and assignment
  TempoMultiplier(const TempoMultiplier &) = delete;
  TempoMultiplier &operator=(const TempoMultiplier &) = delete;

  /**
   * @brief Notification handler called when a TempoEvent is received.
   * Implements the etl::observer interface.
   * @param event The received tempo event (representing one high-resolution tick).
   */
  void notification(Tempo::TempoEvent event) override;

  /**
   * @brief Set the tempo multiplier.
   * @param multiplier The factor to multiply the base tempo by (e.g., 2 for double time). Must be >
   * 0.
   */
  void set_multiplier(int multiplier);

  /**
   * @brief Set the tempo divider.
   * @param divider The factor to divide the base tempo by (e.g., 4 for 16th notes from PPQN). Must
   * be > 0.
   */
  void set_divider(int divider);

  /**
   * @brief Set the swing amount for even-numbered output ticks.
   * @param amount Swing amount (0.0 = no delay, 0.5 = 50% delay towards next tick, etc.). Clamped
   * to [0.0, 1.0).
   */
  void set_even_swing(float amount);

  /**
   * @brief Set the swing amount for odd-numbered output ticks.
   * @param amount Swing amount (0.0 = no delay, 0.5 = 50% delay towards next tick, etc.). Clamped
   * to [0.0, 1.0).
   */
  void set_odd_swing(float amount);

  /**
   * @brief Reset internal counters (e.g., when transport stops/starts).
   */
  void reset();

private:
  /**
   * @brief Recalculate the number of input ticks per output tick based on multiplier/divider.
   */
  void update_ticks_per_output();

  int _multiplier;
  int _divider;
  float _even_swing_amount; // 0.0 to < 1.0
  float _odd_swing_amount;  // 0.0 to < 1.0

  uint32_t _input_ticks_per_output_tick; // How many 96 PPQN ticks form one output tick
  uint32_t _input_tick_counter;          // Counts incoming 96 PPQN ticks since last reset/output
  uint32_t _output_tick_counter;         // Counts outgoing SequencerTickEvents
};

} // namespace Musin::Timing

#endif // SB25_DRUM_TEMPO_MULTIPLIER_H
