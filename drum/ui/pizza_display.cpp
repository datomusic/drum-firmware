#include "drum/ui/pizza_display.h"
#include "drum/drum_pizza_hardware.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace drum {

namespace {

constexpr uint8_t REDUCED_BRIGHTNESS = 100;
constexpr uint32_t DEFAULT_COLOR_CORRECTION = 0xffe080;

Color apply_visual_effects(Color color, float filter_val, float crush_val,
                           absolute_time_t now) {
  if (filter_val < 0.04f && crush_val < 0.04f) {
    return color;
  }

  uint32_t c = static_cast<uint32_t>(color);
  float r = (c >> 16) & 0xFF;
  float g = (c >> 8) & 0xFF;
  float b = c & 0xFF;

  // Desaturation and brightness reduction for filter

  // Fast approximation for grayscale conversion by averaging RGB components.
  float gray = (r + g + b) / 3.0f;
  r = std::lerp(r, gray, (filter_val / 2));
  g = std::lerp(g, gray, (filter_val / 2));
  b = std::lerp(b, gray, (filter_val / 2));

  // Reduce brightness. Scales from 100% down to 20% as filter effect
  // increases.
  constexpr float MIN_FILTER_BRIGHTNESS = 0.2f;
  float brightness_factor = std::lerp(1.0f, MIN_FILTER_BRIGHTNESS, filter_val);
  r *= brightness_factor;
  g *= brightness_factor;
  b *= brightness_factor;

  // Add a random offset to each color channel for the crush effect
  uint32_t time_us = to_us_since_boot(now);
  // A simple pseudo-random generator based on time.
  // Using different prime multipliers for each channel to reduce correlation.
  float r_offset = static_cast<float>((time_us * 13) % 200) * crush_val;
  float g_offset = static_cast<float>((time_us * 17) % 200) * crush_val;
  float b_offset = static_cast<float>((time_us * 19) % 200) * crush_val;

  r -= r_offset;
  g -= g_offset;
  b -= b_offset;

  uint8_t final_r = static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f));
  uint8_t final_g = static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f));
  uint8_t final_b = static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f));

  return Color((final_r << 16) | (final_g << 8) | final_b);
}

} // anonymous namespace

PizzaDisplay::PizzaDisplay(
    drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
        &sequencer_controller_ref,
    musin::timing::TempoHandler &tempo_handler_ref, musin::Logger &logger_ref)
    : _leds(PIZZA_LED_DATA_PIN, musin::drivers::RGBOrder::GRB, MAX_BRIGHTNESS,
            DEFAULT_COLOR_CORRECTION),
      _drumpad_fade_start_times{},
      _sequencer_controller_ref(sequencer_controller_ref),
      _tempo_handler_ref(tempo_handler_ref), _logger_ref(logger_ref) {
  for (size_t i = 0; i < config::NUM_DRUMPADS; ++i) {
    _drumpad_fade_start_times[i] = nil_time;
  }
  for (size_t i = 0; i < SEQUENCER_TRACKS_DISPLAYED; ++i) {
    _track_override_colors[i] = std::nullopt;
  }
}

void PizzaDisplay::notification(musin::timing::TempoEvent) {
  _clock_tick_counter++;
}

void PizzaDisplay::notification(
    drum::Events::SysExTransferStateChangeEvent event) {
  _sysex_transfer_active = event.is_active;
}

void PizzaDisplay::notification(drum::Events::ParameterChangeEvent event) {
  switch (event.param_id) {
  case drum::Parameter::FILTER_FREQUENCY:
    _filter_value = event.value;
    break;
  case drum::Parameter::CRUSH_EFFECT:
    _crush_value = event.value;
    break;
  default:
    // Ignore other parameters
    break;
  }
}

void PizzaDisplay::draw_base_elements(absolute_time_t now) {
  if (_sysex_transfer_active) {
    // When a SysEx transfer is active, flash the play button green
    Color pulse_color =
        _highlight_is_bright ? drum::PizzaDisplay::COLOR_GREEN : Color(0);
    set_play_button_led(pulse_color);
  } else if (_sequencer_controller_ref.is_running()) {
    set_play_button_led(drum::PizzaDisplay::COLOR_WHITE);
  } else {
    // When stopped, pulse the play button in sync with the step highlight
    Color base_color = drum::PizzaDisplay::COLOR_WHITE;
    Color pulse_color =
        _highlight_is_bright
            ? base_color
            : Color(_leds.adjust_color_brightness(
                  static_cast<uint32_t>(base_color), REDUCED_BRIGHTNESS));
    set_play_button_led(pulse_color);
  }

  update_track_override_colors();
  draw_sequencer_state(now);
}

void PizzaDisplay::draw_sequencer_state(absolute_time_t now) {
  const auto &sequencer = _sequencer_controller_ref.get_sequencer();
  const auto &controller = _sequencer_controller_ref;

  bool is_running = controller.is_running();

  for (size_t track_idx = 0; track_idx < config::NUM_TRACKS; ++track_idx) {
    if (track_idx >= SEQUENCER_TRACKS_DISPLAYED)
      continue;

    const auto &track_data = sequencer.get_track(track_idx);

    for (size_t step_idx = 0; step_idx < config::NUM_STEPS_PER_TRACK;
         ++step_idx) {
      if (step_idx >= SEQUENCER_STEPS_DISPLAYED)
        continue;

      const auto &step = track_data.get_step(step_idx);
      Color base_step_color = calculate_step_color(step);
      Color final_color = base_step_color;

      // Apply track override color if active
      if (track_idx < _track_override_colors.size() &&
          _track_override_colors[track_idx].has_value()) {
        final_color = _track_override_colors[track_idx].value();
      }

      // Apply visual effects for filter and crush
      final_color =
          apply_visual_effects(final_color, _filter_value, _crush_value, now);

      // Apply a pulsing highlight to the "cursor" step.
      // When running, this is the step that just played.
      // When stopped, this is the currently selected step.
      bool is_cursor_step =
          (is_running &&
           controller.get_last_played_step_for_track(track_idx).has_value() &&
           step_idx ==
               controller.get_last_played_step_for_track(track_idx).value()) ||
          (!is_running && step_idx == controller.get_current_step());

      if (is_cursor_step && !_sysex_transfer_active) {
        final_color = apply_pulsing_highlight(final_color);
      }

      std::optional<uint32_t> led_index_opt =
          get_sequencer_led_index(track_idx, step_idx);

      if (led_index_opt.has_value()) {
        _leds.set_pixel(led_index_opt.value(),
                        static_cast<uint32_t>(final_color));
      }
    }
  }
}

std::optional<Color>
PizzaDisplay::get_color_for_midi_note(uint8_t midi_note_number) const {
  for (const auto &note_def : config::global_note_definitions) {
    if (note_def.midi_note_number == midi_note_number) {
      return Color(note_def.color);
    }
  }
  return std::nullopt; // MIDI note not found in global definitions
}

void PizzaDisplay::update_track_override_colors() {
  for (uint8_t track_idx = 0; track_idx < SEQUENCER_TRACKS_DISPLAYED;
       ++track_idx) {
    // Check if either the pad is pressed or retrigger mode is active
    if (_sequencer_controller_ref.is_pad_pressed(track_idx) ||
        _sequencer_controller_ref.get_retrigger_mode_for_track(track_idx) > 0) {
      uint8_t active_note =
          _sequencer_controller_ref.get_active_note_for_track(track_idx);
      std::optional<Color> color_opt = get_color_for_midi_note(active_note);
      if (color_opt.has_value()) {
        _track_override_colors[track_idx] = color_opt.value();
      } else {
        // Fallback: if MIDI note not found in global definitions, use black.
        _track_override_colors[track_idx] = Color(0x000000);
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
  ExternalPinState led_pin_state =
      check_external_pin_state(PIZZA_LED_DATA_PIN, _logger_ref);
  uint8_t initial_brightness = (led_pin_state == ExternalPinState::PULL_UP)
                                   ? REDUCED_BRIGHTNESS
                                   : MAX_BRIGHTNESS;
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

void PizzaDisplay::update_highlight_state() {
  uint32_t ticks_per_step =
      _sequencer_controller_ref.get_ticks_per_musical_step();
  if (ticks_per_step == 0) {
    return; // Avoid division by zero if clock is not configured
  }

  // Check if enough ticks have passed to flip the highlight state
  if (_clock_tick_counter - _last_tick_count_for_highlight >= ticks_per_step) {
    _highlight_is_bright = !_highlight_is_bright;
    _last_tick_count_for_highlight = _clock_tick_counter;
  }
}

void PizzaDisplay::update(absolute_time_t now) {
  update_highlight_state();
  draw_base_elements(now);
  draw_animations(now);
  show();
}

void PizzaDisplay::show() {
  _leds.show();
}

void PizzaDisplay::set_brightness(uint8_t brightness) {
  _leds.set_brightness(brightness);
  // Note: Brightness only affects subsequent set_pixel calls in the current
  // WS2812 impl. If immediate effect is desired, the buffer would need to be
  // recalculated.
}

void PizzaDisplay::clear() {
  _leds.clear();
}

void PizzaDisplay::set_led(uint32_t index, Color color) {
  if (index < NUM_LEDS) {
    _leds.set_pixel(index, static_cast<uint32_t>(color));
  }
}

void PizzaDisplay::set_play_button_led(Color color) {
  _leds.set_pixel(LED_PLAY_BUTTON, static_cast<uint32_t>(color));
}

void PizzaDisplay::_set_physical_drumpad_led(uint8_t pad_index, Color color) {
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
    _leds.set_pixel(led_index_opt.value(), static_cast<uint32_t>(color));
  }
}

void PizzaDisplay::set_keypad_led(uint8_t row, uint8_t col, uint8_t intensity) {
  std::optional<uint32_t> led_index_opt = get_keypad_led_index(row, col);

  if (led_index_opt.has_value()) {
    Color color = calculate_intensity_color(intensity);
    _leds.set_pixel(led_index_opt.value(), static_cast<uint32_t>(color));
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

absolute_time_t
PizzaDisplay::get_drumpad_fade_start_time(uint8_t pad_index) const {
  if (pad_index < _drumpad_fade_start_times.size()) {
    return _drumpad_fade_start_times[pad_index];
  }
  return nil_time;
}

void PizzaDisplay::draw_animations(absolute_time_t now) {
  for (uint8_t i = 0; i < config::NUM_DRUMPADS; ++i) {
    uint8_t active_note =
        _sequencer_controller_ref.get_active_note_for_track(i);
    std::optional<Color> base_color_opt = get_color_for_midi_note(active_note);

    Color base_color(0x000000); // Default to black if MIDI note/color not found
    if (base_color_opt.has_value()) {
      base_color = base_color_opt.value();
    }

    Color final_color = base_color;
    absolute_time_t fade_start_time = _drumpad_fade_start_times[i];

    if (!is_nil_time(fade_start_time)) {
      uint64_t time_since_fade_start_us =
          absolute_time_diff_us(fade_start_time, now);
      uint64_t fade_duration_us =
          static_cast<uint64_t>(FADE_DURATION_MS) * 1000;

      if (time_since_fade_start_us < fade_duration_us) {
        float fade_progress =
            std::min(1.0f, static_cast<float>(time_since_fade_start_us) /
                               static_cast<float>(fade_duration_us));
        float current_brightness_factor =
            MIN_FADE_BRIGHTNESS_FACTOR +
            fade_progress * (1.0f - MIN_FADE_BRIGHTNESS_FACTOR);
        uint8_t brightness_value = static_cast<uint8_t>(std::clamp(
            current_brightness_factor * config::DISPLAY_BRIGHTNESS_MAX_VALUE,
            0.0f, config::DISPLAY_BRIGHTNESS_MAX_VALUE));
        final_color = Color(_leds.adjust_color_brightness(
            static_cast<uint32_t>(base_color), brightness_value));
      } else {
        _drumpad_fade_start_times[i] = nil_time;
      }
    }
    _set_physical_drumpad_led(i, final_color);
  }
}

Color PizzaDisplay::calculate_step_color(
    const musin::timing::Step &step) const {
  Color color(0); // Default to black if step disabled or note invalid

  if (step.enabled && step.note.has_value()) {
    std::optional<Color> base_color_opt =
        get_color_for_midi_note(step.note.value());

    if (!base_color_opt.has_value()) {
      return Color(
          0); // MIDI note not found in global definitions, return black
    }
    Color base_color = base_color_opt.value();

    uint8_t brightness = MAX_BRIGHTNESS;
    if (step.velocity.has_value()) {
      uint16_t calculated_brightness =
          static_cast<uint16_t>(step.velocity.value()) *
          VELOCITY_TO_BRIGHTNESS_SCALE;
      brightness = static_cast<uint8_t>(std::min(
          calculated_brightness, static_cast<uint16_t>(MAX_BRIGHTNESS)));
    }

    color = Color(_leds.adjust_color_brightness(
        static_cast<uint32_t>(base_color), brightness));
  }
  return color;
}

Color PizzaDisplay::apply_pulsing_highlight(Color base_color) const {
  uint8_t amount;

  if (_highlight_is_bright) {
    amount = HIGHLIGHT_BLEND_AMOUNT;
  } else {
    amount = ((HIGHLIGHT_BLEND_AMOUNT * REDUCED_BRIGHTNESS) >> 8);
  }
  return base_color.brighter(amount, MAX_BRIGHTNESS);
}

Color PizzaDisplay::calculate_intensity_color(uint8_t intensity) const {
  uint16_t calculated_brightness =
      static_cast<uint16_t>(intensity) * INTENSITY_TO_BRIGHTNESS_SCALE;
  uint8_t brightness_val = static_cast<uint8_t>(
      std::min(calculated_brightness, static_cast<uint16_t>(MAX_BRIGHTNESS)));
  return Color(_leds.adjust_color_brightness(static_cast<uint32_t>(COLOR_WHITE),
                                             brightness_val));
}

} // namespace drum
