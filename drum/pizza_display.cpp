#include "pizza_display.h"
#include "drum_pizza_hardware.h"

#include <algorithm>
#include <array>
#include <cstddef>

extern "C" {
#include "hardware/gpio.h"
#include "pico/time.h"
#include <stdio.h>
}

namespace drum {

namespace {

constexpr auto PULL_CHECK_DELAY_US = 10;
constexpr uint8_t REDUCED_BRIGHTNESS = 100;
constexpr uint32_t DEFAULT_COLOR_CORRECTION = 0xffe080;

enum class ExternalPinState {
  FLOATING,
  PULL_UP,
  PULL_DOWN,
  UNDETERMINED
};

ExternalPinState check_external_pin_state(std::uint32_t gpio, const char *name) {
  gpio_init(gpio);
  gpio_set_dir(gpio, GPIO_IN);

  gpio_disable_pulls(gpio);
  sleep_us(PULL_CHECK_DELAY_US);
  bool initial_read = gpio_get(gpio);

  gpio_pull_up(gpio);
  sleep_us(PULL_CHECK_DELAY_US);
  bool pullup_read = gpio_get(gpio);

  gpio_pull_down(gpio);
  sleep_us(PULL_CHECK_DELAY_US);
  bool pulldown_read = gpio_get(gpio);

  ExternalPinState determined_state;
  const char *state_str;

  if (!initial_read && pullup_read && !pulldown_read) {
    determined_state = ExternalPinState::FLOATING;
    state_str = "Floating";
  } else if (initial_read && pullup_read && !pulldown_read) {
    determined_state = ExternalPinState::FLOATING;
    state_str = "Floating";
  } else if (!initial_read && !pullup_read) {
    determined_state = ExternalPinState::PULL_DOWN;
    state_str = "External Pull-down";
  } else if (initial_read && pulldown_read) {
    determined_state = ExternalPinState::PULL_UP;
    state_str = "External Pull-up";
  } else {
    determined_state = ExternalPinState::UNDETERMINED;
    state_str = "Undetermined / Inconsistent Reads";
  }

  gpio_disable_pulls(gpio);
  sleep_us(PULL_CHECK_DELAY_US);

  return determined_state;
}

} // anonymous namespace

PizzaDisplay::PizzaDisplay(
    drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
        &sequencer_controller_ref,
    musin::timing::TempoHandler &tempo_handler_ref)
    : _leds(PIN_LED_DATA, musin::drivers::RGBOrder::GRB, MAX_BRIGHTNESS, DEFAULT_COLOR_CORRECTION),
      note_colors({0xFF0000, 0xFF0020, 0xFF0040, 0xFF0060, 0xFF1010, 0xFF1020, 0xFF2040, 0xFF2060,
                   0x0000FF, 0x0028FF, 0x0050FF, 0x0078FF, 0x1010FF, 0x1028FF, 0x2050FF, 0x3078FF,
                   0x00FF00, 0x00FF1E, 0x00FF3C, 0x00FF5A, 0x10FF10, 0x10FF1E, 0x10FF3C, 0x20FF5A,
                   0xFFFF00, 0xFFE100, 0xFFC300, 0xFFA500, 0xFFFF20, 0xFFE120, 0xFFC320, 0xFFA520}),
      _drumpad_fade_start_times{}, _sequencer_controller_ref(sequencer_controller_ref),
      _tempo_handler_ref(tempo_handler_ref) {
  for (size_t i = 0; i < config::NUM_DRUMPADS; ++i) {
    _drumpad_fade_start_times[i] = nil_time;
  }
  for (size_t i = 0; i < SEQUENCER_TRACKS_DISPLAYED; ++i) {
    _track_override_colors[i] = std::nullopt;
  }
}

void PizzaDisplay::notification(musin::timing::TempoEvent /* event */) {
  if (!_sequencer_controller_ref.is_running()) {
    _clock_tick_counter++;
  } else {
    _clock_tick_counter = 0;
  }
}

void PizzaDisplay::draw_base_elements() {
  if (_sequencer_controller_ref.is_running()) {
    set_play_button_led(drum::PizzaDisplay::COLOR_WHITE);
  } else {
    uint32_t ticks_per_beat = _sequencer_controller_ref.get_ticks_per_musical_step();
    uint32_t phase_ticks = 0;
    if (ticks_per_beat > 0) {
      phase_ticks = _clock_tick_counter % ticks_per_beat;
    }
    float brightness_factor = 0.0f;
    if (ticks_per_beat > 0) {
      brightness_factor =
          1.0f - (static_cast<float>(phase_ticks) / static_cast<float>(ticks_per_beat));
    }
    _stopped_highlight_factor = std::clamp(brightness_factor, 0.0f, 1.0f);
    uint8_t brightness =
        static_cast<uint8_t>(_stopped_highlight_factor * config::DISPLAY_BRIGHTNESS_MAX_VALUE);
    uint32_t base_color = drum::PizzaDisplay::COLOR_WHITE;
    uint32_t pulse_color = _leds.adjust_color_brightness(base_color, brightness);
    set_play_button_led(pulse_color);
  }

  // The template arguments are resolved because _sequencer_controller_ref
  // is typed with config::NUM_TRACKS and config::NUM_STEPS_PER_TRACK.
  update_track_override_colors();
  draw_sequencer_state(_sequencer_controller_ref.get_sequencer(), _sequencer_controller_ref);
}

void PizzaDisplay::update_track_override_colors() {
  for (uint8_t track_idx = 0; track_idx < SEQUENCER_TRACKS_DISPLAYED; ++track_idx) {
    if (_sequencer_controller_ref.is_pad_pressed(track_idx)) {
      uint8_t active_note = _sequencer_controller_ref.get_active_note_for_track(track_idx);
      _track_override_colors[track_idx] = get_note_color(active_note % NUM_NOTE_COLORS);
    } else {
      _track_override_colors[track_idx] = std::nullopt;
    }
  }
}

void PizzaDisplay::notification(drum::Events::NoteEvent event) {
  if (event.velocity > 0) {
    if (event.track_index < config::NUM_DRUMPADS) {
      this->start_drumpad_fade(event.track_index);
    }
  }
}

bool PizzaDisplay::init() {
  ExternalPinState led_pin_state = check_external_pin_state(PIN_LED_DATA, "LED_DATA");
  uint8_t initial_brightness =
      (led_pin_state == ExternalPinState::PULL_UP) ? REDUCED_BRIGHTNESS : MAX_BRIGHTNESS;
  _leds.set_brightness(initial_brightness);

  if (!_leds.init()) {
    return false;
  }

  gpio_init(PIN_LED_ENABLE);
  gpio_set_dir(PIN_LED_ENABLE, GPIO_OUT);
  gpio_put(PIN_LED_ENABLE, 1);
  clear();
  show();
  return true;
}

void PizzaDisplay::show() {
  _leds.show();
}

void PizzaDisplay::set_brightness(uint8_t brightness) {
  _leds.set_brightness(brightness);
  // Note: Brightness only affects subsequent set_pixel calls in the current WS2812 impl.
  // If immediate effect is desired, the buffer would need to be recalculated.
}

void PizzaDisplay::clear() {
  _leds.clear();
}

void PizzaDisplay::set_led(uint32_t index, uint32_t color) {
  if (index < NUM_LEDS) {
    _leds.set_pixel(index, color);
  }
}

void PizzaDisplay::set_play_button_led(uint32_t color) {
  _leds.set_pixel(LED_PLAY_BUTTON, color);
}

uint32_t PizzaDisplay::get_note_color(uint8_t note_index) const {
  if (note_index < note_colors.size()) {
    return note_colors[note_index];
  }
  return 0;
}

void PizzaDisplay::_set_physical_drumpad_led(uint8_t pad_index, uint32_t color) {
  std::optional<uint32_t> led_index_opt;
  switch (pad_index) {
  case 0:
    led_index_opt = LED_DRUMPAD_1;
    break;
  case 1:
    led_index_opt = LED_DRUMPAD_2;
    break;
  case 2:
    led_index_opt = LED_DRUMPAD_3;
    break;
  case 3:
    led_index_opt = LED_DRUMPAD_4;
    break;
  default:
    return;
  }

  if (led_index_opt.has_value()) {
    _leds.set_pixel(led_index_opt.value(), color);
  }
}

void PizzaDisplay::set_keypad_led(uint8_t row, uint8_t col, uint8_t intensity) {
  std::optional<uint32_t> led_index_opt = get_keypad_led_index(row, col);

  if (led_index_opt.has_value()) {
    uint32_t color = calculate_intensity_color(intensity);
    _leds.set_pixel(led_index_opt.value(), color);
  }
}

// --- Drumpad Fade Implementations ---

void PizzaDisplay::start_drumpad_fade(uint8_t pad_index) {
  if (pad_index < _drumpad_fade_start_times.size()) {
    _drumpad_fade_start_times[pad_index] = get_absolute_time();
  }
}

void PizzaDisplay::clear_drumpad_fade(uint8_t pad_index) {
  if (pad_index < _drumpad_fade_start_times.size()) {
    _drumpad_fade_start_times[pad_index] = nil_time;
  }
}

absolute_time_t PizzaDisplay::get_drumpad_fade_start_time(uint8_t pad_index) const {
  if (pad_index < _drumpad_fade_start_times.size()) {
    return _drumpad_fade_start_times[pad_index];
  }
  return nil_time;
}

void PizzaDisplay::draw_animations(absolute_time_t now) {
  for (uint8_t i = 0; i < config::NUM_DRUMPADS; ++i) {
    uint8_t active_note = _sequencer_controller_ref.get_active_note_for_track(i);
    uint32_t base_color = get_note_color(active_note % NUM_NOTE_COLORS);
    uint32_t final_color = base_color;
    absolute_time_t fade_start_time = _drumpad_fade_start_times[i];

    if (!is_nil_time(fade_start_time)) {
      uint64_t time_since_fade_start_us = absolute_time_diff_us(fade_start_time, now);
      uint64_t fade_duration_us = static_cast<uint64_t>(FADE_DURATION_MS) * 1000;

      if (time_since_fade_start_us < fade_duration_us) {
        float fade_progress = std::min(1.0f, static_cast<float>(time_since_fade_start_us) /
                                                 static_cast<float>(fade_duration_us));
        float current_brightness_factor =
            MIN_FADE_BRIGHTNESS_FACTOR + fade_progress * (1.0f - MIN_FADE_BRIGHTNESS_FACTOR);
        uint8_t brightness_value = static_cast<uint8_t>(
            std::clamp(current_brightness_factor * config::DISPLAY_BRIGHTNESS_MAX_VALUE, 0.0f,
                       config::DISPLAY_BRIGHTNESS_MAX_VALUE));
        final_color = _leds.adjust_color_brightness(base_color, brightness_value);
      } else {
        _drumpad_fade_start_times[i] = nil_time;
      }
    }
    _set_physical_drumpad_led(i, final_color);
  }
}

} // namespace drum
