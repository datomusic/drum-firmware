#ifndef DRUM_FLASH_ACCESS_COORDINATOR_H
#define DRUM_FLASH_ACCESS_COORDINATOR_H

#include <cstdint>

namespace drum {

/**
 * @brief State of the flash access coordinator.
 *
 * Provides mutual exclusion between flash reads (sample loading) and
 * flash writes (sequencer state persistence).
 */
enum class FlashAccessState : uint8_t {
  Idle,           ///< No flash operations in progress
  Reading,        ///< Sample load in progress
  PendingWrite,   ///< Write requested, waiting for read to complete
  PreparingWrite, ///< Pre-buffering audio before write
  Writing,        ///< Flash write in progress
  PendingRead     ///< Read requested, waiting for write to complete
};

/**
 * @brief Coordinates flash read/write operations to prevent conflicts.
 *
 * Flash operations are mutually exclusive - you cannot read while erasing
 * or programming. This coordinator ensures:
 * 1. Sample loads don't start during flash writes
 * 2. Flash writes don't start during sample loads
 * 3. Audio buffers are pre-filled before writes begin
 */
class FlashAccessCoordinator {
public:
  FlashAccessCoordinator() = default;

  /**
   * @brief Request to begin a flash read operation (sample loading).
   * @return true if read can start immediately, false if queued (write in
   * progress)
   */
  bool request_read();

  /**
   * @brief Signal that a flash read operation has completed.
   */
  void read_complete();

  /**
   * @brief Request to begin a flash write operation (state save).
   * @return true if write preparation can start, false if waiting for reads
   */
  bool request_write();

  /**
   * @brief Signal that audio buffers are filled and write can proceed.
   * Call this after pre-buffering audio to survive the flash blackout.
   */
  void buffers_ready();

  /**
   * @brief Signal that a flash write operation has completed.
   */
  void write_complete();

  /**
   * @brief Cancel a pending write request.
   * Use when write is no longer needed (e.g., state became clean).
   */
  void cancel_write();

  /**
   * @brief Get the current coordinator state.
   */
  FlashAccessState get_state() const {
    return state_;
  }

  /**
   * @brief Check if a flash read can start now.
   */
  bool can_read() const;

  /**
   * @brief Check if a flash write is pending or in progress.
   */
  bool is_write_pending() const;

  /**
   * @brief Check if there are queued read requests.
   */
  bool has_pending_reads() const {
    return pending_reads_ > 0;
  }

  /**
   * @brief Get count of active/pending read operations.
   */
  uint8_t active_read_count() const {
    return active_reads_;
  }

private:
  FlashAccessState state_ = FlashAccessState::Idle;
  uint8_t active_reads_ = 0;  ///< Count of concurrent read operations
  uint8_t pending_reads_ = 0; ///< Count of reads queued during write
};

} // namespace drum

#endif // DRUM_FLASH_ACCESS_COORDINATOR_H
