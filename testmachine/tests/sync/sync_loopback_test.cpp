#include "sync_loopback_test.h"

#include <cstdio>

namespace testmachine {

SyncLoopbackTest::SyncLoopbackTest(musin::timing::SyncOut &sync_out,
                                   musin::timing::SyncIn &sync_in)
    : sync_out_(sync_out), sync_in_(sync_in) {}

SyncLoopbackTest::~SyncLoopbackTest() {
  sync_out_.disable();
  sync_in_.remove_observer(*this);
}

void SyncLoopbackTest::start(absolute_time_t now) {
  complete_ = false;
  ticks_sent_ = 0;
  pulses_received_ = 0;
  result_ = {TestStatus::Running, ""};

  next_tick_time_ = now;
  timeout_time_ = delayed_by_ms(now, TIMEOUT_MS);

  // Enable SyncOut and subscribe to SyncIn notifications
  sync_out_.enable();
  sync_in_.add_observer(*this);
}

void SyncLoopbackTest::update(absolute_time_t now) {
  if (complete_) {
    return;
  }

  // Check timeout
  if (time_reached(timeout_time_)) {
    complete_ = true;
    sync_out_.disable();
    sync_in_.remove_observer(*this);

    char msg[64];
    snprintf(msg, sizeof(msg), "timeout: sent %lu ticks, received %lu pulses",
             ticks_sent_, pulses_received_);
    result_ = TestResult::timeout(msg);
    return;
  }

  // Check cable detection after we've sent some ticks
  if (ticks_sent_ > TICKS_PER_PULSE && !sync_in_.is_cable_connected()) {
    complete_ = true;
    sync_out_.disable();
    sync_in_.remove_observer(*this);

    char msg[64];
    snprintf(msg, sizeof(msg), "cable not detected (sent %lu ticks)",
             ticks_sent_);
    result_ = TestResult::failed(msg);
    return;
  }

  // Send clock ticks to SyncOut to generate pulses
  if (ticks_sent_ < target_ticks_ && time_reached(next_tick_time_)) {
    musin::timing::ClockEvent event{musin::timing::ClockSource::INTERNAL};
    sync_out_.notification(event);
    ticks_sent_++;
    next_tick_time_ = delayed_by_ms(now, TICK_INTERVAL_MS);
  }

  // Update SyncIn to process any incoming pulses
  sync_in_.update(now);

  // Check for completion: sent all ticks and received expected pulses
  if (ticks_sent_ >= target_ticks_ && pulses_received_ >= TARGET_PULSES) {
    complete_ = true;
    sync_out_.disable();
    sync_in_.remove_observer(*this);

    char msg[64];
    snprintf(msg, sizeof(msg), "%lu/%lu pulses", pulses_received_,
             TARGET_PULSES);
    result_ = TestResult::passed(msg);
  }
}

void SyncLoopbackTest::notification(musin::timing::ClockEvent event) {
  // Called by SyncIn when a pulse is detected
  if (event.source == musin::timing::ClockSource::EXTERNAL_SYNC) {
    // Only count physical pulses (is_beat=true), not interpolated ticks
    if (event.is_beat) {
      pulses_received_++;
    }
  }
}

bool SyncLoopbackTest::is_complete() const { return complete_; }

TestResult SyncLoopbackTest::get_result() const { return result_; }

void SyncLoopbackTest::reset() {
  complete_ = false;
  ticks_sent_ = 0;
  pulses_received_ = 0;
  sync_out_.disable();
  sync_in_.remove_observer(*this);
  result_ = {TestStatus::NotStarted, ""};
}

} // namespace testmachine
