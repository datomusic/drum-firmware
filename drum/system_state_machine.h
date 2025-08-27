#ifndef DRUM_SYSTEM_STATE_MACHINE_H
#define DRUM_SYSTEM_STATE_MACHINE_H

#include "drum/config.h"
#include "drum/events.h"
#include "etl/observer.h"
#include "pico/time.h"
#include <cstdint>

// Forward declarations to avoid including heavy headers
namespace musin {
class Logger;
namespace timing {
class SyncIn;
class ClockMultiplier;
class InternalClock;
class TempoHandler;
} // namespace timing
} // namespace musin

namespace drum {
class SysExHandler;
class PizzaControls;
template <size_t, size_t> class SequencerController;
class MessageRouter;
class AudioEngine;
class PizzaDisplay;
class MidiManager;

enum class SystemStateId {
  Boot,
  Sequencer,
  FileTransfer,
  FallingAsleep,
  Sleep
};

/**
 * @brief Manages system-wide states and orchestrates behavior.
 *
 * This class centralizes state management, acting as a "Context" that holds
 * references to all major subsystems. It contains the logic for each state
 * internally, removing the need for a separate State pattern implementation
 * and simplifying the main application loop.
 */
class SystemStateMachine
    : public etl::observer<drum::Events::SysExTransferStateChangeEvent> {
public:
  /**
   * @brief Construct a new SystemStateMachine.
   *
   * The constructor takes references to all major subsystems it needs to
   * orchestrate.
   */
  SystemStateMachine(
      musin::Logger &logger, SysExHandler &sysex_handler,
      PizzaControls &pizza_controls, musin::timing::SyncIn &sync_in,
      musin::timing::ClockMultiplier &clock_multiplier,
      SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
          &sequencer_controller,
      MessageRouter &message_router, AudioEngine &audio_engine,
      PizzaDisplay &pizza_display, MidiManager &midi_manager,
      musin::timing::InternalClock &internal_clock,
      musin::timing::TempoHandler &tempo_handler);

  void update(absolute_time_t now);
  SystemStateId get_current_state() const;
  bool transition_to(SystemStateId new_state);
  void notification(drum::Events::SysExTransferStateChangeEvent event) override;

private:
  void update_boot_state(absolute_time_t now);
  void update_sequencer_state(absolute_time_t now);
  void update_file_transfer_state(absolute_time_t now);
  void update_falling_asleep_state(absolute_time_t now);
  void update_sleep_state(absolute_time_t now);

  void handle_state_entry(SystemStateId new_state);
  void handle_state_exit(SystemStateId old_state);

  bool is_valid_transition(SystemStateId from, SystemStateId to) const;

  musin::Logger &logger_;
  SysExHandler &sysex_handler_;
  PizzaControls &pizza_controls_;
  musin::timing::SyncIn &sync_in_;
  musin::timing::ClockMultiplier &clock_multiplier_;
  SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
      &sequencer_controller_;
  MessageRouter &message_router_;
  AudioEngine &audio_engine_;
  PizzaDisplay &pizza_display_;
  MidiManager &midi_manager_;
  musin::timing::InternalClock &internal_clock_;
  musin::timing::TempoHandler &tempo_handler_;

  SystemStateId current_state_id_;
  absolute_time_t state_entry_time_;
};

} // namespace drum

#endif // DRUM_SYSTEM_STATE_MACHINE_H