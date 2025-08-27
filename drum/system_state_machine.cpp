#include "system_state_machine.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"

#include "sequencer_controller.h"

#include "audio_engine.h"
#include "message_router.h"
#include "midi_manager.h"
#include "pizza_controls.h"
#include "sequencer_controller.h"
#include "sysex_handler.h"
#include "ui/pizza_display.h"

#include "musin/hal/logger.h"
#include "musin/midi/midi_output_queue.h"
#include "musin/timing/clock_multiplier.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/tempo_handler.h"
#include "musin/usb/usb.h"

#include <pico/time.h>

namespace drum {

constexpr uint32_t BOOT_DURATION_MS = 2000;
constexpr uint32_t FALL_ASLEEP_DURATION_MS = 5000;

SystemStateMachine::SystemStateMachine(
    musin::Logger &logger, SysExHandler &sysex_handler,
    PizzaControls &pizza_controls, musin::timing::SyncIn &sync_in,
    musin::timing::ClockMultiplier &clock_multiplier,
    SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
        &sequencer_controller,
    MessageRouter &message_router, AudioEngine &audio_engine,
    PizzaDisplay &pizza_display, MidiManager &midi_manager,
    musin::timing::InternalClock &internal_clock,
    musin::timing::TempoHandler &tempo_handler)
    : logger_(logger), sysex_handler_(sysex_handler),
      pizza_controls_(pizza_controls), sync_in_(sync_in),
      clock_multiplier_(clock_multiplier),
      sequencer_controller_(sequencer_controller),
      message_router_(message_router), audio_engine_(audio_engine),
      pizza_display_(pizza_display), midi_manager_(midi_manager),
      internal_clock_(internal_clock), tempo_handler_(tempo_handler),
      current_state_id_(SystemStateId::Boot) {
  handle_state_entry(current_state_id_);
}

void SystemStateMachine::update(absolute_time_t now) {
  switch (current_state_id_) {
  case SystemStateId::Boot:
    update_boot_state(now);
    break;
  case SystemStateId::Sequencer:
    update_sequencer_state(now);
    break;
  case SystemStateId::FileTransfer:
    update_file_transfer_state(now);
    break;
  case SystemStateId::FallingAsleep:
    update_falling_asleep_state(now);
    break;
  case SystemStateId::Sleep:
    update_sleep_state(now);
    break;
  }
}

SystemStateId SystemStateMachine::get_current_state() const {
  return current_state_id_;
}

bool SystemStateMachine::transition_to(SystemStateId new_state) {
  if (!is_valid_transition(current_state_id_, new_state)) {
    logger_.warn("Invalid state transition");
    return false;
  }

  handle_state_exit(current_state_id_);
  current_state_id_ = new_state;
  handle_state_entry(current_state_id_);

  return true;
}

void SystemStateMachine::notification(
    drum::Events::SysExTransferStateChangeEvent event) {
  if (event.is_active) {
    transition_to(SystemStateId::FileTransfer);
  } else {
    transition_to(SystemStateId::Sequencer);
  }
}

void SystemStateMachine::update_boot_state(absolute_time_t now) {
  musin::usb::background_update();
  pizza_display_.update(now);
  midi_manager_.process_input();

  if (time_reached(state_entry_time_) &&
      absolute_time_diff_us(state_entry_time_, now) / 1000 > BOOT_DURATION_MS) {
    transition_to(SystemStateId::Sequencer);
  }
}

void SystemStateMachine::update_sequencer_state(absolute_time_t now) {
  musin::usb::background_update();
  sysex_handler_.update(now);
  pizza_controls_.update(now);
  sync_in_.update(now);
  clock_multiplier_.update(now);
  sequencer_controller_
      .update(); // Checks if a step is due and queues NoteEvents
  message_router_
      .update(); // Drains NoteEvent queue, sending to observers and MIDI
  audio_engine_.process();
  pizza_display_.update(now);
  midi_manager_.process_input();
  internal_clock_.update(now);
  tempo_handler_.update();
  musin::midi::process_midi_output_queue(
      logger_); // Pass logger to queue processing
  sleep_us(10);
}

void SystemStateMachine::update_file_transfer_state(absolute_time_t now) {
  musin::usb::background_update();
  sysex_handler_.update(now);
  pizza_display_.update(now); // Keep display alive for progress updates
  midi_manager_.process_input();
  musin::midi::process_midi_output_queue(logger_); // For sending ACKs
}

void SystemStateMachine::update_falling_asleep_state(absolute_time_t now) {
  pizza_display_.update(now);
  midi_manager_.process_input();
  musin::midi::process_midi_output_queue(logger_);
  sleep_us(10);

  if (time_reached(state_entry_time_) &&
      absolute_time_diff_us(state_entry_time_, now) / 1000 >
          FALL_ASLEEP_DURATION_MS) {
    transition_to(SystemStateId::Sleep);
  }
}

void SystemStateMachine::update_sleep_state(
    [[maybe_unused]] absolute_time_t now) {
  constexpr uint32_t MUX_IO_PIN = DATO_SUBMARINE_ADC_PIN;
  // Now check for button press to wake
  if (!gpio_get(MUX_IO_PIN)) {
    logger_.debug("Playbutton pressed - triggering reset");
    // Intentionally enter infinite loop to trigger watchdog reset in 500ms
    // This provides a clean wake mechanism by resetting the entire system
    while (true) {
      // No watchdog update - intentional reset via watchdog timeout
    }
  }

  sleep_us(10000);
  watchdog_update();
}

void SystemStateMachine::handle_state_entry(SystemStateId new_state) {
  state_entry_time_ = get_absolute_time();
  switch (new_state) {
  case SystemStateId::Boot:
    pizza_display_.start_boot_animation();
    break;
  case SystemStateId::Sequencer:
    pizza_display_.switch_to_sequencer_mode();
    break;
  case SystemStateId::FileTransfer:
    pizza_display_.switch_to_file_transfer_mode();
    break;
  case SystemStateId::FallingAsleep:
    audio_engine_.mute();
    pizza_display_.start_sleep_mode();
    break;
  case SystemStateId::Sleep:
    audio_engine_.deinit();
    pizza_display_.deinit();

    // Configure MUX for playbutton wake detection
    logger_.debug("Configuring MUX for playbutton wake");

    // Initialize MUX address pins as outputs
    gpio_init(DATO_SUBMARINE_MUX_ADDR0_PIN);
    gpio_set_dir(DATO_SUBMARINE_MUX_ADDR0_PIN, GPIO_OUT);
    gpio_init(DATO_SUBMARINE_MUX_ADDR1_PIN);
    gpio_set_dir(DATO_SUBMARINE_MUX_ADDR1_PIN, GPIO_OUT);
    gpio_init(DATO_SUBMARINE_MUX_ADDR2_PIN);
    gpio_set_dir(DATO_SUBMARINE_MUX_ADDR2_PIN, GPIO_OUT);
    gpio_init(DATO_SUBMARINE_MUX_ADDR3_PIN);
    gpio_set_dir(DATO_SUBMARINE_MUX_ADDR3_PIN, GPIO_OUT);

    // Set MUX address to playbutton channel
    constexpr uint32_t PLAYBUTTON_ADDRESS = 5;
    gpio_put(DATO_SUBMARINE_MUX_ADDR0_PIN, PLAYBUTTON_ADDRESS & 0x01);
    gpio_put(DATO_SUBMARINE_MUX_ADDR1_PIN, (PLAYBUTTON_ADDRESS >> 1) & 0x01);
    gpio_put(DATO_SUBMARINE_MUX_ADDR2_PIN, (PLAYBUTTON_ADDRESS >> 2) & 0x01);
    gpio_put(DATO_SUBMARINE_MUX_ADDR3_PIN, (PLAYBUTTON_ADDRESS >> 3) & 0x01);

    // Configure MUX IO pin for input with pullup
    constexpr uint32_t MUX_IO_PIN = DATO_SUBMARINE_ADC_PIN;
    gpio_init(MUX_IO_PIN);
    gpio_set_dir(MUX_IO_PIN, GPIO_IN);

    // Enable watchdog for wake reset mechanism
    watchdog_enable(500, false);

    set_sys_clock_48mhz();

    logger_.debug("MUX configured for playbutton wake - waiting for button "
                  "release first");

    // Wait for button release first
    while (!gpio_get(MUX_IO_PIN)) {
      sleep_us(10000);
      watchdog_update();
    }
    break;
  }
}

void SystemStateMachine::handle_state_exit(
    [[maybe_unused]] SystemStateId old_state) {
  // Nothing to do here for now, but it's good practice to have it.
}

bool SystemStateMachine::is_valid_transition(SystemStateId from,
                                             SystemStateId to) const {
  switch (from) {
  case SystemStateId::Boot:
    return (to == SystemStateId::Sequencer);
  case SystemStateId::Sequencer:
    return (to == SystemStateId::FileTransfer ||
            to == SystemStateId::FallingAsleep);
  case SystemStateId::FileTransfer:
    return (to == SystemStateId::Sequencer);
  case SystemStateId::FallingAsleep:
    return (to == SystemStateId::Sleep);
  case SystemStateId::Sleep:
    return false; // Wake-up should reset the system
  default:
    return false;
  }
}

} // namespace drum
