#include "state_implementations.h"
#include "drum/drum_pizza_hardware.h"
#include "drum/ui/pizza_display.h"
#include "musin/hal/logger.h"
#include "system_state_machine.h"

extern "C" {
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "pico/time.h"
}

namespace drum {

// --- BootState Implementation ---

void BootState::enter(musin::Logger &logger) {
  logger.debug("Entering Boot state");
  boot_start_time_ = get_absolute_time();
}

void BootState::update(musin::Logger &logger, SystemStateMachine &state_machine,
                       absolute_time_t now) {
  // Transition to Sequencer after 2 seconds (temporary solution)
  uint64_t elapsed_us =
      to_us_since_boot(now) - to_us_since_boot(boot_start_time_);
  if (elapsed_us > 2000000) { // 2 seconds
    logger.debug("Boot timeout - transitioning to Sequencer");
    state_machine.transition_to(SystemStateId::Sequencer);
  }
}

void BootState::exit(musin::Logger &logger) {
  logger.debug("Exiting Boot state");
}

// --- SequencerState Implementation ---

void SequencerState::enter(musin::Logger &logger) {
  logger.debug("Entering Sequencer state");
}

void SequencerState::update([[maybe_unused]] musin::Logger &logger,
                            [[maybe_unused]] SystemStateMachine &state_machine,
                            [[maybe_unused]] absolute_time_t now) {
  // TODO Phase 4: Handle sequencer-specific logic and events
  // Minimal implementation for now
}

void SequencerState::exit(musin::Logger &logger) {
  logger.debug("Exiting Sequencer state");
}

// --- FileTransferState Implementation ---

void FileTransferState::enter(musin::Logger &logger) {
  logger.debug("Entering FileTransfer state");
  in_debounce_period_ = false;
}

void FileTransferState::update(musin::Logger &logger,
                               SystemStateMachine &state_machine,
                               absolute_time_t now) {
  // Check if we're in debounce period and should transition back to Sequencer
  if (in_debounce_period_) {
    if (absolute_time_diff_us(completion_time_, now) > DEBOUNCE_MS * 1000) {
      logger.debug(
          "FileTransfer debounce expired - transitioning to Sequencer");
      state_machine.transition_to(SystemStateId::Sequencer);
    }
  }
}

void FileTransferState::exit(musin::Logger &logger) {
  logger.debug("Exiting FileTransfer state");
  in_debounce_period_ = false;
}

void FileTransferState::on_transfer_complete() {
  completion_time_ = get_absolute_time();
  in_debounce_period_ = true;
}

void FileTransferState::on_transfer_start() {
  in_debounce_period_ = false;
}

// --- FallingAsleepState Implementation ---

void FallingAsleepState::enter(musin::Logger &logger) {
  logger.debug("Entering FallingAsleep state");
  fallback_timeout_ =
      make_timeout_time_ms(500); // 500ms to match SleepDisplayMode dimming
}

void FallingAsleepState::update(musin::Logger &logger,
                                SystemStateMachine &state_machine,
                                absolute_time_t now) {
  // Check timeout (500ms to match display dimming)
  if (absolute_time_diff_us(now, fallback_timeout_) <= 0) {
    logger.debug("Display dimming complete - transitioning to Sleep");
    state_machine.transition_to(SystemStateId::Sleep);
    return;
  }
}

void FallingAsleepState::exit(musin::Logger &logger) {
  logger.debug("Exiting FallingAsleep state");
}

// --- SleepState Implementation ---

void SleepState::enter(musin::Logger &logger) {
  logger.debug("Entering Sleep state");
  // Note: fadeout already started in FallingAsleepState

  // Configure MUX for playbutton wake detection
  logger.debug("Configuring MUX for playbutton wake");

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

  // Initialize button release detection state
  waiting_for_button_release_ = true;
  logger.debug("MUX configured - will wait for button release in update loop");
}

void SleepState::update(musin::Logger &logger,
                        [[maybe_unused]] SystemStateMachine &state_machine,
                        [[maybe_unused]] absolute_time_t now) {
  constexpr uint32_t MUX_IO_PIN = DATO_SUBMARINE_ADC_PIN;

  if (waiting_for_button_release_) {
    // Non-blocking wait for button release
    if (gpio_get(MUX_IO_PIN)) {
      waiting_for_button_release_ = false;
      logger.debug("Button released - now monitoring for wake press");
    }
    // Don't check for wake press while waiting for release
    sleep_us(10000);
    watchdog_update();
    return;
  }

  // Now check for button press to wake
  if (!gpio_get(MUX_IO_PIN)) {
    logger.debug("Playbutton pressed - triggering reset");
    // Intentionally enter infinite loop to trigger watchdog reset in 500ms
    // This provides a clean wake mechanism by resetting the entire system
    while (true) {
      // No watchdog update - intentional reset via watchdog timeout
    }
  }

  sleep_us(10000);
  watchdog_update();
}

void SleepState::exit(musin::Logger &logger) {
  logger.debug("Exiting Sleep state");
}

} // namespace drum
