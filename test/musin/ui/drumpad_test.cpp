#include "musin/ui/drumpad.h"
#include "pico/time.h"

#include <catch2/catch_test_macros.hpp>

using musin::ui::Drumpad;
using musin::ui::DrumpadState;
using musin::ui::RetriggerMode;

namespace {

// active_low = false so the raw ADC value passed to update() is used directly,
// keeping the threshold arithmetic in the tests easy to follow.
constexpr musin::ui::DrumpadConfig test_config = {
    .noise_threshold = 150,
    .trigger_threshold = 800,
    .high_pressure_threshold = 2500,
    .active_low = false,
    .debounce_time_us = 5000,
    .hold_time_us = 50000,
    .max_velocity_time_us = 50000,
    .min_velocity_time_us = 100};

// Drive the pad from Idle to Peaking, emitting a Press once trigger is crossed.
void press_to_peaking(Drumpad &pad, uint16_t adc) {
  set_mock_time_us(1000);
  pad.update(adc); // Idle -> Rising (adc >= noise_threshold)
  pad.update(adc); // Rising -> Peaking, Press fired
  REQUIRE(pad.get_current_state() == DrumpadState::Peaking);
  REQUIRE(pad.was_pressed());
}

// Hold past hold_time_us so the pad enters Holding, then run one more update so
// the Holding-state logic (which sets the retrigger mode) executes.
void hold_until_mode_resolved(Drumpad &pad, uint16_t adc) {
  advance_mock_time_us(60000);
  pad.update(adc); // Peaking -> Holding (Hold event)
  REQUIRE(pad.is_held());
  pad.update(adc); // Holding-state logic resolves retrigger mode
}

} // namespace

TEST_CASE("Drumpad starts with retrigger Off") {
  Drumpad pad(0, test_config);
  pad.init();
  REQUIRE(pad.get_retrigger_mode() == RetriggerMode::Off);
  REQUIRE(pad.get_current_state() == DrumpadState::Idle);
}

TEST_CASE("Quick tap never engages retrigger") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000);

  pad.update(0); // Peaking -> DebouncingRelease (below noise, before hold_time)
  REQUIRE(pad.get_retrigger_mode() == RetriggerMode::Off);

  advance_mock_time_us(6000);
  pad.update(0); // DebouncingRelease -> Release -> Idle
  REQUIRE(pad.was_released());
  REQUIRE(pad.get_retrigger_mode() == RetriggerMode::Off);
}

TEST_CASE("Holding above trigger engages Single") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000);
  REQUIRE(pad.get_retrigger_mode() == RetriggerMode::Single);
}

TEST_CASE("Holding above high pressure engages Double") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 3000);
  hold_until_mode_resolved(pad, 3000);
  REQUIRE(pad.get_retrigger_mode() == RetriggerMode::Double);
}

// Regression test for the bug where a pad crossed trigger (lighting the ring)
// but then settled below trigger before reaching Holding, leaving retrigger Off
// while the ring stayed lit.
TEST_CASE("Holding below trigger after a press still engages Single") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000);        // crosses trigger, Press fired
  hold_until_mode_resolved(pad, 500); // settles to 150..799 range while held
  REQUIRE(pad.get_retrigger_mode() == RetriggerMode::Single);
}

TEST_CASE("Double is preserved when pressure drops below trigger") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 3000);
  hold_until_mode_resolved(pad, 3000);
  REQUIRE(pad.get_retrigger_mode() == RetriggerMode::Double);

  pad.update(500); // pressure relaxes below trigger; mode must not downgrade
  REQUIRE(pad.get_retrigger_mode() == RetriggerMode::Double);
}

TEST_CASE("Retrigger clears on Release") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000);
  REQUIRE(pad.get_retrigger_mode() == RetriggerMode::Single);

  pad.update(0); // Holding -> Falling
  pad.update(0); // Falling -> DebouncingRelease
  advance_mock_time_us(6000);
  pad.update(0); // DebouncingRelease -> Release -> Idle
  REQUIRE(pad.was_released());
  REQUIRE(pad.get_retrigger_mode() == RetriggerMode::Off);
}
