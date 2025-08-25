#include "state_implementations.h"
#include "drum/drum_pizza_hardware.h"
#include "drum/ui/pizza_display.h"
#include "musin/hal/logger.h"
#include "system_state_machine.h"

extern "C" {
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "pico/time.h"
}

namespace drum {

// --- BootState Implementation ---

void BootState::enter(PizzaDisplay &display, musin::Logger &logger) {
  logger.debug("Entering Boot state");
  // TODO Phase 4: Start boot animation via event system
  // For now, directly call display (will be removed in Phase 3)
  display.start_boot_animation();
  boot_start_time_ = get_absolute_time();
}

void BootState::update(PizzaDisplay &display, musin::Logger &logger,
                       SystemStateMachine &state_machine, absolute_time_t now) {
  (void)display; // Unused in this method
  // Transition to Sequencer after 2 seconds (temporary solution)
  uint64_t elapsed_us =
      to_us_since_boot(now) - to_us_since_boot(boot_start_time_);
  if (elapsed_us > 2000000) { // 2 seconds
    logger.debug("Boot timeout - transitioning to Sequencer");
    state_machine.transition_to(SystemStateId::Sequencer);
  }
}

void BootState::exit(PizzaDisplay &display, musin::Logger &logger) {
  (void)display; // Unused in this method
  logger.debug("Exiting Boot state");
}

// --- SequencerState Implementation ---

void SequencerState::enter(PizzaDisplay &display, musin::Logger &logger) {
  logger.debug("Entering Sequencer state");
  // TODO Phase 4: Switch to sequencer mode via event system
  // For now, directly call display (will be removed in Phase 3)
  display.switch_to_sequencer_mode();
}

void SequencerState::update([[maybe_unused]] PizzaDisplay &display,
                            [[maybe_unused]] musin::Logger &logger,
                            [[maybe_unused]] SystemStateMachine &state_machine,
                            [[maybe_unused]] absolute_time_t now) {
  // TODO Phase 4: Handle sequencer-specific logic and events
  // Minimal implementation for now
}

void SequencerState::exit(PizzaDisplay &display, musin::Logger &logger) {
  (void)display; // Unused in this method
  logger.debug("Exiting Sequencer state");
}

// --- FileTransferState Implementation ---

void FileTransferState::enter(PizzaDisplay &display, musin::Logger &logger) {
  logger.debug("Entering FileTransfer state");
  // TODO Phase 4: Switch to file transfer mode via event system
  // For now, directly call display (will be removed in Phase 3)
  display.switch_to_file_transfer_mode();
}

void FileTransferState::update(
    [[maybe_unused]] PizzaDisplay &display,
    [[maybe_unused]] musin::Logger &logger,
    [[maybe_unused]] SystemStateMachine &state_machine,
    [[maybe_unused]] absolute_time_t now) {
  // TODO Phase 4: Handle file transfer-specific logic
  // Minimal implementation for now
}

void FileTransferState::exit(PizzaDisplay &display, musin::Logger &logger) {
  (void)display; // Unused in this method
  logger.debug("Exiting FileTransfer state");
}

// --- FallingAsleepState Implementation ---

void FallingAsleepState::enter(PizzaDisplay &display, musin::Logger &logger) {
  logger.debug("Entering FallingAsleep state");
  display.start_sleep_mode(); // Start the fadeout
  fallback_timeout_ = make_timeout_time_ms(750); // 750ms fallback (longer than 500ms fadeout)
}

void FallingAsleepState::update([[maybe_unused]] PizzaDisplay &display, musin::Logger &logger,
                                SystemStateMachine &state_machine, absolute_time_t now) {
  // Check timeout (750ms fallback)
  if (absolute_time_diff_us(now, fallback_timeout_) <= 0) {
    logger.debug("Timeout reached - transitioning to Sleep");
    state_machine.transition_to(SystemStateId::Sleep);
    return;
  }
}

void FallingAsleepState::exit(PizzaDisplay &display, musin::Logger &logger) {
  (void)display; // Unused in this method
  logger.debug("Exiting FallingAsleep state");
}

// --- SleepState Implementation ---

void SleepState::enter(PizzaDisplay &display, musin::Logger &logger) {
  logger.debug("Entering Sleep state");
  // Note: fadeout already started in FallingAsleepState

  // Configure MUX for playbutton wake detection
  logger.debug("Configuring MUX for playbutton wake");
  display.deinit(); // Turn off LED enable pin

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
  gpio_pull_up(MUX_IO_PIN);

  // Enable watchdog for wake reset mechanism
  watchdog_enable(500, false);

  logger.debug(
      "MUX configured for playbutton wake - waiting for button release first");
}

void SleepState::update([[maybe_unused]] PizzaDisplay &display,
                        musin::Logger &logger,
                        [[maybe_unused]] SystemStateMachine &state_machine,
                        [[maybe_unused]] absolute_time_t now) {
  constexpr uint32_t MUX_IO_PIN = DATO_SUBMARINE_ADC_PIN;

  // Wait for button release first
  if (!gpio_get(MUX_IO_PIN)) {
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

void SleepState::exit(PizzaDisplay &display, musin::Logger &logger) {
  (void)display; // Unused in this method
  logger.debug("Exiting Sleep state");
}

} // namespace drum