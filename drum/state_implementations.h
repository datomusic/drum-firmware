#ifndef DRUM_STATE_IMPLEMENTATIONS_H
#define DRUM_STATE_IMPLEMENTATIONS_H

#include "system_state.h"

namespace drum {

/**
 * @brief Boot state - handles system initialization and boot animation.
 */
class BootState : public SystemState {
public:
  void enter(PizzaDisplay &display, musin::Logger &logger) override;
  void update(PizzaDisplay &display, musin::Logger &logger,
              SystemStateMachine &state_machine, absolute_time_t now) override;
  void exit(PizzaDisplay &display, musin::Logger &logger) override;
  SystemStateId get_id() const override {
    return SystemStateId::Boot;
  }

private:
  absolute_time_t boot_start_time_;
};

/**
 * @brief Sequencer state - handles normal sequencer operation.
 */
class SequencerState : public SystemState {
public:
  void enter(PizzaDisplay &display, musin::Logger &logger) override;
  void update(PizzaDisplay &display, musin::Logger &logger,
              SystemStateMachine &state_machine, absolute_time_t now) override;
  void exit(PizzaDisplay &display, musin::Logger &logger) override;
  SystemStateId get_id() const override {
    return SystemStateId::Sequencer;
  }
};

/**
 * @brief File transfer state - handles minimal systems for file transfer
 * performance.
 */
class FileTransferState : public SystemState {
public:
  void enter(PizzaDisplay &display, musin::Logger &logger) override;
  void update(PizzaDisplay &display, musin::Logger &logger,
              SystemStateMachine &state_machine, absolute_time_t now) override;
  void exit(PizzaDisplay &display, musin::Logger &logger) override;
  SystemStateId get_id() const override {
    return SystemStateId::FileTransfer;
  }
};

/**
 * @brief Falling asleep state - handles UI fadeout and transition to sleep.
 */
class FallingAsleepState : public SystemState {
public:
  void enter(PizzaDisplay &display, musin::Logger &logger) override;
  void update(PizzaDisplay &display, musin::Logger &logger,
              SystemStateMachine &state_machine, absolute_time_t now) override;
  void exit(PizzaDisplay &display, musin::Logger &logger) override;
  SystemStateId get_id() const override {
    return SystemStateId::FallingAsleep;
  }

private:
  absolute_time_t fallback_timeout_;
};

/**
 * @brief Sleep state - handles device sleep mode and wake detection.
 */
class SleepState : public SystemState {
public:
  void enter(PizzaDisplay &display, musin::Logger &logger) override;
  void update(PizzaDisplay &display, musin::Logger &logger,
              SystemStateMachine &state_machine, absolute_time_t now) override;
  void exit(PizzaDisplay &display, musin::Logger &logger) override;
  SystemStateId get_id() const override {
    return SystemStateId::Sleep;
  }
};

} // namespace drum

#endif // DRUM_STATE_IMPLEMENTATIONS_H