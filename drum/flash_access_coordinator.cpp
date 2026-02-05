#include "flash_access_coordinator.h"

namespace drum {

bool FlashAccessCoordinator::request_read() {
  switch (state_) {
  case FlashAccessState::Idle:
  case FlashAccessState::Reading:
    // Can start reading immediately
    active_reads_++;
    state_ = FlashAccessState::Reading;
    return true;

  case FlashAccessState::PendingWrite:
    // Read already in progress, allow additional read
    active_reads_++;
    return true;

  case FlashAccessState::PreparingWrite:
  case FlashAccessState::Writing:
    // Cannot read during write preparation or active write
    pending_reads_++;
    if (state_ == FlashAccessState::Writing) {
      state_ = FlashAccessState::PendingRead;
    }
    return false;

  case FlashAccessState::PendingRead:
    // Already have pending reads, add to queue
    pending_reads_++;
    return false;
  }

  return false;
}

void FlashAccessCoordinator::read_complete() {
  if (active_reads_ > 0) {
    active_reads_--;
  }

  if (active_reads_ == 0) {
    switch (state_) {
    case FlashAccessState::Reading:
      state_ = FlashAccessState::Idle;
      break;

    case FlashAccessState::PendingWrite:
      // All reads done, can now prepare for write
      state_ = FlashAccessState::PreparingWrite;
      break;

    default:
      // Unexpected state, but safe to ignore
      break;
    }
  }
}

bool FlashAccessCoordinator::request_write() {
  switch (state_) {
  case FlashAccessState::Idle:
    // Can start write preparation immediately
    state_ = FlashAccessState::PreparingWrite;
    return true;

  case FlashAccessState::Reading:
    // Must wait for reads to complete
    state_ = FlashAccessState::PendingWrite;
    return false;

  case FlashAccessState::PendingWrite:
  case FlashAccessState::PreparingWrite:
  case FlashAccessState::Writing:
  case FlashAccessState::PendingRead:
    // Write already pending or in progress
    return false;
  }

  return false;
}

void FlashAccessCoordinator::buffers_ready() {
  if (state_ == FlashAccessState::PreparingWrite) {
    state_ = FlashAccessState::Writing;
  }
}

void FlashAccessCoordinator::write_complete() {
  switch (state_) {
  case FlashAccessState::Writing:
    state_ = FlashAccessState::Idle;
    break;

  case FlashAccessState::PendingRead:
    // Process pending reads
    if (pending_reads_ > 0) {
      active_reads_ = pending_reads_;
      pending_reads_ = 0;
      state_ = FlashAccessState::Reading;
    } else {
      state_ = FlashAccessState::Idle;
    }
    break;

  default:
    // Unexpected state
    state_ = FlashAccessState::Idle;
    break;
  }
}

void FlashAccessCoordinator::cancel_write() {
  switch (state_) {
  case FlashAccessState::PendingWrite:
    // Return to reading state if reads are active
    if (active_reads_ > 0) {
      state_ = FlashAccessState::Reading;
    } else {
      state_ = FlashAccessState::Idle;
    }
    break;

  case FlashAccessState::PreparingWrite:
    state_ = FlashAccessState::Idle;
    break;

  default:
    // Cannot cancel once writing has started
    break;
  }
}

bool FlashAccessCoordinator::can_read() const {
  switch (state_) {
  case FlashAccessState::Idle:
  case FlashAccessState::Reading:
  case FlashAccessState::PendingWrite:
    return true;

  case FlashAccessState::PreparingWrite:
  case FlashAccessState::Writing:
  case FlashAccessState::PendingRead:
    return false;
  }

  return false;
}

bool FlashAccessCoordinator::is_write_pending() const {
  switch (state_) {
  case FlashAccessState::PendingWrite:
  case FlashAccessState::PreparingWrite:
  case FlashAccessState::Writing:
  case FlashAccessState::PendingRead:
    return true;

  case FlashAccessState::Idle:
  case FlashAccessState::Reading:
    return false;
  }

  return false;
}

} // namespace drum
