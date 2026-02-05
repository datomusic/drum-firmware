#include "catch2/catch_test_macros.hpp"
#include "drum/flash_access_coordinator.h"

using drum::FlashAccessCoordinator;
using drum::FlashAccessState;

TEST_CASE("FlashAccessCoordinator starts in Idle state",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;
  REQUIRE(coordinator.get_state() == FlashAccessState::Idle);
  REQUIRE(coordinator.can_read());
  REQUIRE_FALSE(coordinator.is_write_pending());
  REQUIRE_FALSE(coordinator.has_pending_reads());
}

TEST_CASE("FlashAccessCoordinator allows read from Idle",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;

  REQUIRE(coordinator.request_read());
  REQUIRE(coordinator.get_state() == FlashAccessState::Reading);
  REQUIRE(coordinator.active_read_count() == 1);
}

TEST_CASE("FlashAccessCoordinator allows multiple concurrent reads",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;

  REQUIRE(coordinator.request_read());
  REQUIRE(coordinator.request_read());
  REQUIRE(coordinator.get_state() == FlashAccessState::Reading);
  REQUIRE(coordinator.active_read_count() == 2);

  coordinator.read_complete();
  REQUIRE(coordinator.get_state() == FlashAccessState::Reading);
  REQUIRE(coordinator.active_read_count() == 1);

  coordinator.read_complete();
  REQUIRE(coordinator.get_state() == FlashAccessState::Idle);
  REQUIRE(coordinator.active_read_count() == 0);
}

TEST_CASE("FlashAccessCoordinator allows write from Idle",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;

  REQUIRE(coordinator.request_write());
  REQUIRE(coordinator.get_state() == FlashAccessState::PreparingWrite);
  REQUIRE(coordinator.is_write_pending());
  REQUIRE_FALSE(coordinator.can_read());
}

TEST_CASE("FlashAccessCoordinator write waits for active reads",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;

  // Start a read
  REQUIRE(coordinator.request_read());
  REQUIRE(coordinator.get_state() == FlashAccessState::Reading);

  // Request write - should be queued
  REQUIRE_FALSE(coordinator.request_write());
  REQUIRE(coordinator.get_state() == FlashAccessState::PendingWrite);
  REQUIRE(coordinator.is_write_pending());

  // New read should still be allowed during PendingWrite
  REQUIRE(coordinator.can_read());
  REQUIRE(coordinator.request_read());
  REQUIRE(coordinator.active_read_count() == 2);

  // Complete first read
  coordinator.read_complete();
  REQUIRE(coordinator.get_state() == FlashAccessState::PendingWrite);

  // Complete second read - should transition to PreparingWrite
  coordinator.read_complete();
  REQUIRE(coordinator.get_state() == FlashAccessState::PreparingWrite);
}

TEST_CASE("FlashAccessCoordinator buffers_ready transitions to Writing",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;

  coordinator.request_write();
  REQUIRE(coordinator.get_state() == FlashAccessState::PreparingWrite);

  coordinator.buffers_ready();
  REQUIRE(coordinator.get_state() == FlashAccessState::Writing);
}

TEST_CASE("FlashAccessCoordinator write_complete returns to Idle",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;

  coordinator.request_write();
  coordinator.buffers_ready();
  REQUIRE(coordinator.get_state() == FlashAccessState::Writing);

  coordinator.write_complete();
  REQUIRE(coordinator.get_state() == FlashAccessState::Idle);
  REQUIRE_FALSE(coordinator.is_write_pending());
  REQUIRE(coordinator.can_read());
}

TEST_CASE("FlashAccessCoordinator read during write is queued",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;

  // Start write
  coordinator.request_write();
  coordinator.buffers_ready();
  REQUIRE(coordinator.get_state() == FlashAccessState::Writing);

  // Request read during write - should be queued
  REQUIRE_FALSE(coordinator.request_read());
  REQUIRE(coordinator.get_state() == FlashAccessState::PendingRead);
  REQUIRE(coordinator.has_pending_reads());

  // Request another read
  REQUIRE_FALSE(coordinator.request_read());

  // Complete write - pending reads should become active
  coordinator.write_complete();
  REQUIRE(coordinator.get_state() == FlashAccessState::Reading);
  REQUIRE(coordinator.active_read_count() == 2);
  REQUIRE_FALSE(coordinator.has_pending_reads());
}

TEST_CASE("FlashAccessCoordinator cancel_write from PendingWrite",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;

  // Start read
  coordinator.request_read();

  // Request write (will be pending)
  coordinator.request_write();
  REQUIRE(coordinator.get_state() == FlashAccessState::PendingWrite);

  // Cancel write
  coordinator.cancel_write();
  REQUIRE(coordinator.get_state() == FlashAccessState::Reading);
  REQUIRE_FALSE(coordinator.is_write_pending());
}

TEST_CASE("FlashAccessCoordinator cancel_write from PreparingWrite",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;

  coordinator.request_write();
  REQUIRE(coordinator.get_state() == FlashAccessState::PreparingWrite);

  coordinator.cancel_write();
  REQUIRE(coordinator.get_state() == FlashAccessState::Idle);
}

TEST_CASE("FlashAccessCoordinator cannot cancel during active write",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;

  coordinator.request_write();
  coordinator.buffers_ready();
  REQUIRE(coordinator.get_state() == FlashAccessState::Writing);

  // Cancel should be ignored during active write
  coordinator.cancel_write();
  REQUIRE(coordinator.get_state() == FlashAccessState::Writing);
}

TEST_CASE("FlashAccessCoordinator full read-write-read cycle",
          "[flash_access_coordinator]") {
  FlashAccessCoordinator coordinator;

  // Start with reads
  REQUIRE(coordinator.request_read());
  REQUIRE(coordinator.request_read());

  // Request write (queued)
  REQUIRE_FALSE(coordinator.request_write());
  REQUIRE(coordinator.get_state() == FlashAccessState::PendingWrite);

  // Complete reads
  coordinator.read_complete();
  coordinator.read_complete();
  REQUIRE(coordinator.get_state() == FlashAccessState::PreparingWrite);

  // Buffers ready, start write
  coordinator.buffers_ready();
  REQUIRE(coordinator.get_state() == FlashAccessState::Writing);

  // New read requested during write
  REQUIRE_FALSE(coordinator.request_read());
  REQUIRE(coordinator.get_state() == FlashAccessState::PendingRead);

  // Complete write
  coordinator.write_complete();
  REQUIRE(coordinator.get_state() == FlashAccessState::Reading);

  // Complete the pending read
  coordinator.read_complete();
  REQUIRE(coordinator.get_state() == FlashAccessState::Idle);
}
