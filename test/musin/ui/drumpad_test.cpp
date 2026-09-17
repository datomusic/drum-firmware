#include "musin/ui/drumpad.h"
#include "pico/time.h"

#include <catch2/catch_test_macros.hpp>

using musin::ui::Drumpad;
using musin::ui::DrumpadState;
using musin::ui::PressureLevel;

namespace {

// active_low = false so the raw ADC value passed to update() is used directly,
// keeping the threshold arithmetic in the tests easy to follow.
constexpr musin::ui::DrumpadConfig test_config = {.noise_threshold = 150,
                                                  .trigger_threshold = 800,
                                                  .high_pressure_threshold =
                                                      2500,
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
// the Holding-state logic (which sets the pressure level) executes.
void hold_until_mode_resolved(Drumpad &pad, uint16_t adc) {
  advance_mock_time_us(60000);
  pad.update(adc); // Peaking -> Holding (Hold event)
  REQUIRE(pad.is_held());
  pad.update(adc); // Holding-state logic resolves pressure level
}

} // namespace

TEST_CASE("Drumpad starts with pressure level None") {
  Drumpad pad(0, test_config);
  pad.init();
  REQUIRE(pad.get_pressure_level() == PressureLevel::None);
  REQUIRE(pad.get_current_state() == DrumpadState::Idle);
}

TEST_CASE("Quick tap never reports a pressure level") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000);

  pad.update(0); // Peaking -> DebouncingRelease (below noise, before hold_time)
  REQUIRE(pad.get_pressure_level() == PressureLevel::None);

  advance_mock_time_us(6000);
  pad.update(0); // DebouncingRelease -> Release -> Idle
  REQUIRE(pad.was_released());
  REQUIRE(pad.get_pressure_level() == PressureLevel::None);
}

TEST_CASE("Holding above trigger reports Light") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000);
  REQUIRE(pad.get_pressure_level() == PressureLevel::Light);
}

TEST_CASE("Holding above high pressure reports Hard") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 3000);
  hold_until_mode_resolved(pad, 3000);
  REQUIRE(pad.get_pressure_level() == PressureLevel::Hard);
}

// Regression test for the bug where a pad crossed trigger (lighting the ring)
// but then settled below trigger before reaching Holding, leaving pressure
// level None while the ring stayed lit.
TEST_CASE("Holding below trigger after a press still reports Light") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000);        // crosses trigger, Press fired
  hold_until_mode_resolved(pad, 500); // settles to 150..799 range while held
  REQUIRE(pad.get_pressure_level() == PressureLevel::Light);
}

// Same regression, but the pad is scanned while pressure sits below trigger
// before hold_time_us expires. Previously this update moved Peaking to
// Falling, a dead end where the hold timer no longer ran, so the hold never
// engaged even though the ring stayed lit until release.
TEST_CASE("Scan below trigger before hold expiry still reports Light") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000); // crosses trigger, Press fired

  advance_mock_time_us(10000);
  pad.update(500); // pressure sags below trigger, above noise, before hold
  REQUIRE(pad.get_pressure_level() == PressureLevel::None);

  advance_mock_time_us(50000);
  pad.update(500); // hold timer expires while contact persists
  REQUIRE(pad.is_held());
  pad.update(500); // Holding-state logic resolves pressure level
  REQUIRE(pad.get_pressure_level() == PressureLevel::Light);
}

TEST_CASE("Hard is preserved when pressure drops below trigger") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 3000);
  hold_until_mode_resolved(pad, 3000);
  REQUIRE(pad.get_pressure_level() == PressureLevel::Hard);

  pad.update(500); // pressure relaxes below trigger; mode must not downgrade
  REQUIRE(pad.get_pressure_level() == PressureLevel::Hard);
}

// Bug: Falling is a dead end for pressure. Once Holding drops below trigger
// into Falling, a subsequent rise back to high pressure should upgrade the
// pressure level to Hard, but Falling only ever watches for the noise
// floor, so the mode is stuck at whatever it was when Falling began.
TEST_CASE("Rising to high pressure while Falling upgrades to Hard") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000);
  REQUIRE(pad.get_pressure_level() == PressureLevel::Light);

  pad.update(400); // Holding -> Falling (below trigger, above noise)
  REQUIRE(pad.get_current_state() == DrumpadState::Falling);

  pad.update(3000); // Pressure spikes back up to high pressure
  REQUIRE(pad.get_pressure_level() == PressureLevel::Hard);
}

// Bug: a brief dip below noise while Peaking (before hold_time_us expires)
// sends the pad through DebouncingRelease back into Falling once contact
// resumes. Falling never checks the hold timer, so the pad can remain
// pressed indefinitely without ever reaching Holding.
TEST_CASE(
    "Contact resuming after a brief dip below noise still reaches Holding") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000); // crosses trigger, Press fired

  pad.update(0); // Peaking -> DebouncingRelease (brief dip below noise)
  REQUIRE(pad.get_current_state() == DrumpadState::DebouncingRelease);

  pad.update(
      1000); // Contact resumes above trigger -> DebouncingRelease -> Falling
  REQUIRE(pad.get_current_state() == DrumpadState::Falling);

  advance_mock_time_us(60000);
  pad.update(1000); // Held long enough that hold_time_us has elapsed
  REQUIRE(pad.is_held());
  pad.update(1000); // Holding-state logic resolves pressure level
  REQUIRE(pad.get_pressure_level() == PressureLevel::Light);
}

TEST_CASE("Pressure level clears on Release") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000);
  REQUIRE(pad.get_pressure_level() == PressureLevel::Light);

  pad.update(0); // Holding -> Falling
  pad.update(0); // Falling -> DebouncingRelease
  advance_mock_time_us(6000);
  pad.update(0); // DebouncingRelease -> Release -> Idle
  REQUIRE(pad.was_released());
  REQUIRE(pad.get_pressure_level() == PressureLevel::None);
}

TEST_CASE("Easing off from high pressure downgrades Hard to Light") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 3000);
  hold_until_mode_resolved(pad, 3000);
  REQUIRE(pad.get_pressure_level() == PressureLevel::Hard);

  pad.update(1000); // still above trigger, but no longer high pressure
  REQUIRE(pad.get_pressure_level() == PressureLevel::Light);

  pad.update(3000); // pressing hard again re-engages Hard
  REQUIRE(pad.get_pressure_level() == PressureLevel::Hard);
}

TEST_CASE("Contact bounce before hold expiry returns to Peaking") {
  Drumpad pad(0, test_config);
  pad.init();
  press_to_peaking(pad, 1000);

  pad.update(0);    // Peaking -> DebouncingRelease
  pad.update(1000); // DebouncingRelease -> Falling
  advance_mock_time_us(10000);
  pad.update(1000); // Falling -> Peaking, hold timer still measured from Press
  REQUIRE(pad.get_current_state() == DrumpadState::Peaking);
  REQUIRE(pad.get_pressure_level() == PressureLevel::None);

  advance_mock_time_us(50000);
  pad.update(1000); // hold time since the original Press has elapsed
  REQUIRE(pad.is_held());
}
