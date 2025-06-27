#include "pizza_display.h"
#include "drum_pizza_hardware.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace drum {

namespace {

constexpr uint8_t REDUCED_BRIGHTNESS = 100;
constexpr uint32_t DEFAULT_COLOR_CORRECTION = 0xffe080;

} // anonymous namespace

PizzaDisplay::PizzaDisplay(
    drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
        &sequencer_controller_ref,
    musin::timing::TempoHandler &tempo_handler_ref)
    : _leds(PIZZA_LED_DATA_PIN, musin::drivers::RGBOrder::GRB, MAX_BRIGHTNESS,
            DEFAULT_COLOR_CORRECTION),
      _drumpad_fade_start_times{}, _sequencer_controller_ref(sequencer_controller_ref),
      _tempo_handler_ref(tempo_handler_ref) {
  for (size_t i = 0; i < config::NUM_DRUMPADS; ++i) {
    _drumpad_fade_start_times[i] = nil_time;
  }
  for (size_t i = 0; i < SEQUENCER_TRACKS_DISPLAYED; ++i) {
    _track_override_colors[i] = std::nullopt;
  }
}

void PizzaDisplay::notification(musin::timing::TempoEvent) {
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

  update_track_override_colors();
  draw_sequencer_state();
}

void PizzaDisplay::draw_sequencer_state() {
  const auto &sequencer = _sequencer_controller_ref.get_sequencer();
  const auto &controller = _sequencer_controller_ref;

  bool is_running = controller.is_running();

  for (size_t track_idx = 0; track_idx < config::NUM_TRACKS; ++track_idx) {
    if (track_idx >= SEQUENCER_TRACKS_DISPLAYED)
      continue;

    const auto &track_data = sequencer.get_track(track_idx);

    for (size_t step_idx = 0; step_idx < config::NUM_STEPS_PER_TRACK; ++step_idx) {
      if (step_idx >= SEQUENCER_STEPS_DISPLAYED)
        continue;

      const auto &step = track_data.get_step(step_idx);
      uint32_t base_step_color = calculate_step_color(step);
      uint32_t final_color = base_step_color;

      // Apply track override color if active
      if (track_idx < _track_override_colors.size() &&
          _track_override_colors[track_idx].has_value()) {
        final_color = _track_override_colors[track_idx].value();
      }

      // Apply highlighting for the currently playing step (on top of base or override color)
      std::optional<size_t> just_played_step = controller.get_last_played_step_for_track(track_idx);
      if (is_running && just_played_step.has_value() && step_idx == just_played_step.value()) {
        final_color = apply_highlight(final_color);
      }

      if (!is_running && step_idx == controller.get_current_step()) {
        final_color = apply_fading_highlight(final_color, _stopped_highlight_factor);
      }

      std::optional<uint32_t> led_index_opt = get_sequencer_led_index(track_idx, step_idx);

      if (led_index_opt.has_value()) {
        _leds.set_pixel(led_index_opt.value(), final_color);
      }
    }
  }
}

std::optional<uint32_t> PizzaDisplay::get_color_for_midi_note(uint8_t midi_note_number) const {
  for (const auto &note_def : config::global_note_definitions) {
    if (note_def.midi_note_number == midi_note_number) {
      return note_def.color;
    }
  }
  return std::nullopt; // MIDI note not found in global definitions
}

void PizzaDisplay::update_track_override_colors() {
  for (uint8_t track_idx = 0; track_idx < SEQUENCER_TRACKS_DISPLAYED; ++track_idx) {
    // Check if either the pad is pressed or retrigger mode is active
    if (_sequencer_controller_ref.is_pad_pressed(track_idx) ||
        _sequencer_controller_ref.get_retrigger_mode_for_track(track_idx) > 0) {
      uint8_t active_note = _sequencer_controller_ref.get_active_note_for_track(track_idx);
      std::optional<uint32_t> color_opt = get_color_for_midi_note(active_note);
      if (color_opt.has_value()) {
        _track_override_colors[track_idx] = color_opt.value();
      } else {
        // Fallback: if MIDI note not found in global definitions, use black.
        _track_override_colors[track_idx] = 0x000000;
      }
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
  ExternalPinState led_pin_state = check_external_pin_state(PIZZA_LED_DATA_PIN, "LED_DATA");
  uint8_t initial_brightness =
      (led_pin_state == ExternalPinState::PULL_UP) ? REDUCED_BRIGHTNESS : MAX_BRIGHTNESS;
  _leds.set_brightness(initial_brightness);

  if (!_leds.init()) {
    return false;
  }

  gpio_init(PIZZA_LED_ENABLE_PIN);
  gpio_set_dir(PIZZA_LED_ENABLE_PIN, GPIO_OUT);
  gpio_put(PIZZA_LED_ENABLE_PIN, 1);
  clear();
  show();
  return true;
}

void PizzaDisplay::update(absolute_time_t now) {
  draw_base_elements();
  draw_animations(now);
  show();
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
    std::optional<uint32_t> base_color_opt = get_color_for_midi_note(active_note);

    uint32_t base_color = 0x000000; // Default to black if MIDI note/color not found
    if (base_color_opt.has_value()) {
      base_color = base_color_opt.value();
    }

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

uint32_t PizzaDisplay::calculate_step_color(const musin::timing::Step &step) const {
  uint32_t color = 0; // Default to black if step disabled or note invalid

  if (step.enabled && step.note.has_value()) {
    std::optional<uint32_t> base_color_opt = get_color_for_midi_note(step.note.value());

    if (!base_color_opt.has_value()) {
      return 0; // MIDI note not found in global definitions, return black
    }
    uint32_t base_color = base_color_opt.value();

    uint8_t brightness = MAX_BRIGHTNESS;
    if (step.velocity.has_value()) {
      uint16_t calculated_brightness =
          static_cast<uint16_t>(step.velocity.value()) * VELOCITY_TO_BRIGHTNESS_SCALE;
      brightness = static_cast<uint8_t>(
          std::min(calculated_brightness, static_cast<uint16_t>(MAX_BRIGHTNESS)));
    }

    color = _leds.adjust_color_brightness(base_color, brightness);
  }
  return color;
}

uint32_t PizzaDisplay::apply_highlight(uint32_t color) const {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;

  r = static_cast<uint8_t>(
      std::min<int>(MAX_BRIGHTNESS, static_cast<int>(r) + HIGHLIGHT_BLEND_AMOUNT));
  g = static_cast<uint8_t>(
      std::min<int>(MAX_BRIGHTNESS, static_cast<int>(g) + HIGHLIGHT_BLEND_AMOUNT));
  b = static_cast<uint8_t>(
      std::min<int>(MAX_BRIGHTNESS, static_cast<int>(b) + HIGHLIGHT_BLEND_AMOUNT));
  return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

uint32_t PizzaDisplay::apply_fading_highlight(uint32_t color, float highlight_factor) const {
  uint8_t base_r = (color >> 16) & 0xFF;
  uint8_t base_g = (color >> 8) & 0xFF;
  uint8_t base_b = color & 0xFF;

  // Target highlight color is white
  constexpr uint8_t highlight_r = 0xFF;
  constexpr uint8_t highlight_g = 0xFF;
  constexpr uint8_t highlight_b = 0xFF;

  // Scale factor for integer blending (0-255)
  uint8_t blend_amount = static_cast<uint8_t>(std::clamp(highlight_factor * 255.0f, 0.0f, 255.0f));

  // Linear interpolation using integer math (lerp)
  uint8_t final_r = static_cast<uint8_t>((static_cast<uint32_t>(base_r) * (255 - blend_amount) +
                                          static_cast<uint32_t>(highlight_r) * blend_amount) /
                                         255);
  uint8_t final_g = static_cast<uint8_t>((static_cast<uint32_t>(base_g) * (255 - blend_amount) +
                                          static_cast<uint32_t>(highlight_g) * blend_amount) /
                                         255);
  uint8_t final_b = static_cast<uint8_t>((static_cast<uint32_t>(base_b) * (255 - blend_amount) +
                                          static_cast<uint32_t>(highlight_b) * blend_amount) /
                                         255);

  return (static_cast<uint32_t>(final_r) << 16) | (static_cast<uint32_t>(final_g) << 8) | final_b;
}

uint32_t PizzaDisplay::calculate_intensity_color(uint8_t intensity) const {
  uint16_t calculated_brightness = static_cast<uint16_t>(intensity) * INTENSITY_TO_BRIGHTNESS_SCALE;
  uint8_t brightness_val =
      static_cast<uint8_t>(std::min(calculated_brightness, static_cast<uint16_t>(MAX_BRIGHTNESS)));
  return _leds.adjust_color_brightness(COLOR_WHITE, brightness_val);
}

} // namespace drum
