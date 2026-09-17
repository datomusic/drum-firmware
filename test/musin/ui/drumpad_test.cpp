#include "musin/ui/drumpad.h"
#include "pico/time.h"

#include "etl/observer.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using musin::ui::Drumpad;
using musin::ui::DrumpadEvent;
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
                                                  .min_velocity_time_us = 100,
                                                  .pressure_hysteresis = 2};

// Records every DrumpadEvent notified by a Drumpad, in order, so tests can
// assert on both event content and emission order.
struct EventRecorder : public etl::observer<DrumpadEvent> {
  std::vector<DrumpadEvent> events;

  void notification(DrumpadEvent event) override {
    events.push_back(event);
  }
};

// Mirrors the pressure-mapping formula from the API contract so tests never
// hardcode magic 7-bit values: linear map of the (already active_low
// corrected) ADC value from [noise_threshold, ADC_MAX_VALUE] to [0, 127],
// clamped below noise_threshold, integer truncation.
constexpr uint8_t expected_pressure(uint16_t adc, uint16_t noise_threshold) {
  if (adc < noise_threshold) {
    return 0;
  }
  return static_cast<uint8_t>(
      (static_cast<uint32_t>(adc - noise_threshold) * 127) /
      (musin::hal::ADC_MAX_VALUE - noise_threshold));
}

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

TEST_CASE("No Pressure event before the pad is held") {
  Drumpad pad(0, test_config);
  pad.init();
  EventRecorder recorder;
  pad.add_observer(recorder);

  press_to_peaking(pad, 1000);
  advance_mock_time_us(10000); // still well short of hold_time_us
  pad.update(1000);            // still Peaking, no Hold yet
  REQUIRE(pad.get_current_state() == DrumpadState::Peaking);

  pad.update(0); // Peaking -> DebouncingRelease (quick tap, never held)
  advance_mock_time_us(6000);
  pad.update(0); // DebouncingRelease -> Release -> Idle
  REQUIRE(pad.was_released());

  for (const auto &event : recorder.events) {
    REQUIRE(event.type != DrumpadEvent::Type::Pressure);
  }
}

TEST_CASE("First update in Holding emits a Pressure event with the mapped "
          "value") {
  Drumpad pad(0, test_config);
  pad.init();
  EventRecorder recorder;
  pad.add_observer(recorder);

  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000);

  REQUIRE_FALSE(recorder.events.empty());
  const DrumpadEvent &pressure_event = recorder.events.back();
  REQUIRE(pressure_event.type == DrumpadEvent::Type::Pressure);
  REQUIRE(pressure_event.pad_index == 0);
  REQUIRE(pressure_event.velocity == std::nullopt);
  REQUIRE(pressure_event.raw_value == 1000);
  REQUIRE(pressure_event.pressure.has_value());
  REQUIRE(pressure_event.pressure.value() ==
          expected_pressure(1000, test_config.noise_threshold));
}

TEST_CASE("Jitter smaller than hysteresis is silent") {
  Drumpad pad(0, test_config);
  pad.init();
  EventRecorder recorder;
  pad.add_observer(recorder);

  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000); // first Pressure emitted here

  size_t events_before = recorder.events.size();
  pad.update(1001); // tiny jitter, still Holding, maps to the same 7-bit value
  REQUIRE(pad.get_current_state() == DrumpadState::Holding);
  REQUIRE(recorder.events.size() == events_before);
}

TEST_CASE("Change of at least hysteresis emits a new value; pressing harder "
          "raises it, easing off lowers it") {
  Drumpad pad(0, test_config);
  pad.init();
  EventRecorder recorder;
  pad.add_observer(recorder);

  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000);
  uint8_t baseline = expected_pressure(1000, test_config.noise_threshold);

  pad.update(1500); // pressing harder
  REQUIRE(pad.get_current_state() == DrumpadState::Holding);
  REQUIRE(recorder.events.back().type == DrumpadEvent::Type::Pressure);
  uint8_t higher = expected_pressure(1500, test_config.noise_threshold);
  REQUIRE(recorder.events.back().pressure == higher);
  REQUIRE(higher > baseline);

  pad.update(1000); // easing off back down
  REQUIRE(recorder.events.back().type == DrumpadEvent::Type::Pressure);
  REQUIRE(recorder.events.back().pressure == baseline);
  REQUIRE(baseline < higher);
}

TEST_CASE("Pressure keeps following the ADC while Falling") {
  Drumpad pad(0, test_config);
  pad.init();
  EventRecorder recorder;
  pad.add_observer(recorder);

  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000);

  pad.update(400); // Holding -> Falling (below trigger 800, above noise 150)
  REQUIRE(pad.get_current_state() == DrumpadState::Falling);
  REQUIRE(recorder.events.back().type == DrumpadEvent::Type::Pressure);
  REQUIRE(recorder.events.back().pressure ==
          expected_pressure(400, test_config.noise_threshold));

  pad.update(200); // still Falling, pressure keeps dropping towards 0
  REQUIRE(pad.get_current_state() == DrumpadState::Falling);
  REQUIRE(recorder.events.back().type == DrumpadEvent::Type::Pressure);
  REQUIRE(recorder.events.back().pressure ==
          expected_pressure(200, test_config.noise_threshold));
}

TEST_CASE(
    "Release emits Pressure 0 before the Release event when last value was "
    "non-zero") {
  Drumpad pad(0, test_config);
  pad.init();
  EventRecorder recorder;
  pad.add_observer(recorder);

  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000); // Pressure(27) emitted

  pad.update(185); // Holding -> Falling; pressure drops to 1, well above
                   // hysteresis, so it is emitted
  REQUIRE(pad.get_current_state() == DrumpadState::Falling);
  REQUIRE(recorder.events.back().type == DrumpadEvent::Type::Pressure);
  uint8_t low_pressure = expected_pressure(185, test_config.noise_threshold);
  REQUIRE(recorder.events.back().pressure == low_pressure);

  pad.update(100); // Falling -> DebouncingRelease; pressure computes to 0,
                   // but that is within hysteresis of the last emitted
                   // value, so nothing new is emitted here
  REQUIRE(pad.get_current_state() == DrumpadState::DebouncingRelease);
  size_t events_before_release = recorder.events.size();

  advance_mock_time_us(6000);
  pad.update(100); // DebouncingRelease -> Release -> Idle
  REQUIRE(pad.was_released());

  REQUIRE(recorder.events.size() == events_before_release + 2);
  const DrumpadEvent &forced_zero = recorder.events[recorder.events.size() - 2];
  REQUIRE(forced_zero.type == DrumpadEvent::Type::Pressure);
  REQUIRE(forced_zero.pressure.has_value());
  REQUIRE(forced_zero.pressure.value() == 0);
  REQUIRE(recorder.events.back().type == DrumpadEvent::Type::Release);
}

TEST_CASE("No Pressure 0 on release if nothing was emitted") {
  Drumpad pad(0, test_config);
  pad.init();
  EventRecorder recorder;
  pad.add_observer(recorder);

  press_to_peaking(pad, 1000);
  pad.update(0); // Peaking -> DebouncingRelease (quick tap, never held)
  advance_mock_time_us(6000);
  pad.update(0); // DebouncingRelease -> Release -> Idle
  REQUIRE(pad.was_released());

  REQUIRE(recorder.events.size() == 2);
  REQUIRE(recorder.events[0].type == DrumpadEvent::Type::Press);
  REQUIRE(recorder.events[1].type == DrumpadEvent::Type::Release);
}

TEST_CASE("pressure_hysteresis of 0 disables pressure events entirely") {
  musin::ui::DrumpadConfig config_no_pressure = test_config;
  config_no_pressure.pressure_hysteresis = 0;
  Drumpad pad(0, config_no_pressure);
  pad.init();
  EventRecorder recorder;
  pad.add_observer(recorder);

  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000);
  pad.update(3000); // vary pressure substantially
  pad.update(400);  // and again, crossing into Falling

  pad.update(0); // Falling -> DebouncingRelease
  advance_mock_time_us(6000);
  pad.update(0); // DebouncingRelease -> Release -> Idle
  REQUIRE(pad.was_released());

  for (const auto &event : recorder.events) {
    REQUIRE(event.type != DrumpadEvent::Type::Pressure);
  }
}

TEST_CASE(
    "After a release, the next press's first Holding update emits again") {
  Drumpad pad(0, test_config);
  pad.init();
  EventRecorder recorder;
  pad.add_observer(recorder);

  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000);
  pad.update(0); // Holding -> Falling
  pad.update(0); // Falling -> DebouncingRelease
  advance_mock_time_us(6000);
  pad.update(0); // DebouncingRelease -> Release -> Idle
  REQUIRE(pad.was_released());

  press_to_peaking(pad, 1000);
  hold_until_mode_resolved(pad, 1000);

  REQUIRE(recorder.events.back().type == DrumpadEvent::Type::Pressure);
  REQUIRE(recorder.events.back().pressure ==
          expected_pressure(1000, test_config.noise_threshold));
}
