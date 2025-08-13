#include "state_implementations.h"
#include "drum/ui/pizza_display.h"
#include "system_context.h"
#include "system_state_machine.h"

namespace drum {

// --- BootState Implementation ---

void BootState::enter(SystemContext &context) {
  context.logger.debug("Entering Boot state");
  // TODO Phase 4: Start boot animation via event system
  // For now, directly call display (will be removed in Phase 3)
  context.display.start_boot_animation();
  boot_start_time_ = get_absolute_time();
}

void BootState::update(SystemContext &context, absolute_time_t now) {
  // Transition to Sequencer after 2 seconds (temporary solution)
  uint64_t elapsed_us =
      to_us_since_boot(now) - to_us_since_boot(boot_start_time_);
  if (elapsed_us > 2000000 && context.state_machine) { // 2 seconds
    context.logger.debug("Boot timeout - transitioning to Sequencer");
    context.state_machine->transition_to(SystemStateId::Sequencer);
  }
}

void BootState::exit(SystemContext &context) {
  context.logger.debug("Exiting Boot state");
}

// --- SequencerState Implementation ---

void SequencerState::enter(SystemContext &context) {
  context.logger.debug("Entering Sequencer state");
  // TODO Phase 4: Switch to sequencer mode via event system
  // For now, directly call display (will be removed in Phase 3)
  context.display.switch_to_sequencer_mode();
}

void SequencerState::update([[maybe_unused]] SystemContext &context,
                            [[maybe_unused]] absolute_time_t now) {
  // TODO Phase 4: Handle sequencer-specific logic and events
  // Minimal implementation for now
}

void SequencerState::exit(SystemContext &context) {
  context.logger.debug("Exiting Sequencer state");
}

// --- FileTransferState Implementation ---

void FileTransferState::enter(SystemContext &context) {
  context.logger.debug("Entering FileTransfer state");
  // TODO Phase 4: Switch to file transfer mode via event system
  // For now, directly call display (will be removed in Phase 3)
  context.display.switch_to_file_transfer_mode();
}

void FileTransferState::update([[maybe_unused]] SystemContext &context,
                               [[maybe_unused]] absolute_time_t now) {
  // TODO Phase 4: Handle file transfer-specific logic
  // Minimal implementation for now
}

void FileTransferState::exit(SystemContext &context) {
  context.logger.debug("Exiting FileTransfer state");
}

// --- SleepState Implementation ---

void SleepState::enter(SystemContext &context) {
  context.logger.debug("Entering Sleep state");
  // TODO Phase 2: Use SleepManager abstraction instead of direct display calls
  // For now, directly call display (will be removed in Phase 2)
  context.display.start_sleep_mode();
}

void SleepState::update([[maybe_unused]] SystemContext &context,
                        [[maybe_unused]] absolute_time_t now) {
  // TODO Phase 2: Use hardware abstractions for sleep/wake logic
  // TODO Phase 4: Handle wake events properly
  // Minimal implementation for now - no infinite loops!
}

void SleepState::exit(SystemContext &context) {
  context.logger.debug("Exiting Sleep state");
}

} // namespace drum