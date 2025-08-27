#include "drum/ui/display_mode.h"
#include "drum/drum_pizza_hardware.h"
#include "drum/ui/pizza_display.h"

namespace drum::ui {

namespace {

// This helper function was previously in pizza_display.cpp
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

// --- DisplayMode Base Implementation ---

void DisplayMode::on_enter([[maybe_unused]] PizzaDisplay &display) {
  // Default implementation - do nothing
}

// --- SequencerDisplayMode Implementation ---

SequencerDisplayMode::SequencerDisplayMode(
    drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
        &sequencer_controller,
    musin::timing::TempoHandler &tempo_handler)
    : _sequencer_controller_ref(sequencer_controller),
      _tempo_handler_ref(tempo_handler) {
}

void SequencerDisplayMode::draw(PizzaDisplay &display, absolute_time_t now) {
  display.update_highlight_state();
  draw_base_elements(display, now);
  draw_animations(display, now);
}

void SequencerDisplayMode::draw_base_elements(PizzaDisplay &display,
                                              absolute_time_t now) {
  // Determine base color based on clock source
  Color base_color = (_tempo_handler_ref.get_clock_source() ==
                      musin::timing::ClockSource::MIDI)
                         ? Color(config::COLOR_MIDI_CLOCK_LISTENER)
                         : drum::PizzaDisplay::COLOR_WHITE;

  if (_sequencer_controller_ref.is_running()) {
    display.set_play_button_led(base_color);
  } else {
    // When stopped, pulse the play button in sync with the step highlight
    Color pulse_color = display._highlight_is_bright
                            ? base_color
                            : Color(display._leds.adjust_color_brightness(
                                  static_cast<uint32_t>(base_color),
                                  PizzaDisplay::REDUCED_BRIGHTNESS));
    display.set_play_button_led(pulse_color);
  }

  update_track_override_colors(display);
  draw_sequencer_state(display, now);
}

void SequencerDisplayMode::draw_sequencer_state(PizzaDisplay &display,
                                                absolute_time_t now) {
  const auto &sequencer = _sequencer_controller_ref.get_sequencer();
  const auto &controller = _sequencer_controller_ref;

  bool is_running = controller.is_running();

  for (size_t track_idx = 0; track_idx < config::NUM_TRACKS; ++track_idx) {
    if (track_idx >= PizzaDisplay::SEQUENCER_TRACKS_DISPLAYED)
      continue;

    const auto &track_data = sequencer.get_track(track_idx);

    for (size_t step_idx = 0; step_idx < config::NUM_STEPS_PER_TRACK;
         ++step_idx) {
      if (step_idx >= PizzaDisplay::SEQUENCER_STEPS_DISPLAYED)
        continue;

      const auto &step = track_data.get_step(step_idx);
      Color base_step_color = calculate_step_color(display, step);
      Color final_color = base_step_color;

      // Apply track override color if active
      if (track_idx < display._track_override_colors.size() &&
          display._track_override_colors[track_idx].has_value()) {
        final_color = display._track_override_colors[track_idx].value();
      }

      // Apply visual effects for filter and crush
      final_color = apply_visual_effects(final_color, display._filter_value,
                                         display._crush_value, now);

      bool is_cursor_step =
          (is_running &&
           controller.get_last_played_step_for_track(track_idx).has_value() &&
           step_idx ==
               controller.get_last_played_step_for_track(track_idx).value()) ||
          (!is_running && step_idx == controller.get_current_step());

      if (is_cursor_step) {
        final_color = apply_pulsing_highlight(display, final_color);
      }

      std::optional<uint32_t> led_index_opt =
          display.get_sequencer_led_index(track_idx, step_idx);

      if (led_index_opt.has_value()) {
        display._leds.set_pixel(led_index_opt.value(),
                                static_cast<uint32_t>(final_color));
      }
    }
  }
}

void SequencerDisplayMode::update_track_override_colors(PizzaDisplay &display) {
  for (uint8_t track_idx = 0;
       track_idx < PizzaDisplay::SEQUENCER_TRACKS_DISPLAYED; ++track_idx) {
    if (_sequencer_controller_ref.is_pad_pressed(track_idx) ||
        _sequencer_controller_ref.get_retrigger_mode_for_track(track_idx) > 0) {
      uint8_t active_note =
          _sequencer_controller_ref.get_active_note_for_track(track_idx);
      std::optional<Color> color_opt =
          display.get_color_for_midi_note(active_note);
      if (color_opt.has_value()) {
        display._track_override_colors[track_idx] = color_opt.value();
      } else {
        display._track_override_colors[track_idx] = Color(0x000000);
      }
    } else {
      display._track_override_colors[track_idx] = std::nullopt;
    }
  }
}

void SequencerDisplayMode::draw_animations(PizzaDisplay &display,
                                           absolute_time_t now) {
  for (uint8_t i = 0; i < config::NUM_DRUMPADS; ++i) {
    uint8_t active_note =
        _sequencer_controller_ref.get_active_note_for_track(i);
    std::optional<Color> base_color_opt =
        display.get_color_for_midi_note(active_note);

    Color base_color(0x000000);
    if (base_color_opt.has_value()) {
      base_color = base_color_opt.value();
    }

    Color final_color = base_color;
    absolute_time_t fade_start_time = display._drumpad_fade_start_times[i];

    if (!is_nil_time(fade_start_time)) {
      // Calculate fade progress using proper timing
      uint64_t current_time_us = to_us_since_boot(now);
      uint64_t fade_start_us = to_us_since_boot(fade_start_time);
      uint64_t fade_duration_us =
          static_cast<uint64_t>(PizzaDisplay::FADE_DURATION_MS) * 1000;

      if (current_time_us > fade_start_us &&
          (current_time_us - fade_start_us) < fade_duration_us) {
        // Active fade - interpolate from 50% to 100% brightness
        uint64_t elapsed_us = current_time_us - fade_start_us;
        float fade_progress = static_cast<float>(elapsed_us) /
                              static_cast<float>(fade_duration_us);
        float brightness_factor = 0.5f + (fade_progress * 0.5f); // 50% to 100%

        // Apply brightness factor to RGB values
        uint32_t base_rgb = static_cast<uint32_t>(base_color);
        uint8_t r =
            static_cast<uint8_t>(((base_rgb >> 16) & 0xFF) * brightness_factor);
        uint8_t g =
            static_cast<uint8_t>(((base_rgb >> 8) & 0xFF) * brightness_factor);
        uint8_t b = static_cast<uint8_t>((base_rgb & 0xFF) * brightness_factor);
        final_color = Color((r << 16) | (g << 8) | b);
      } else if (current_time_us > fade_start_us) {
        // Fade expired - clear it and return to full brightness
        display._drumpad_fade_start_times[i] = nil_time;
        final_color = base_color;
      } else {
        // Edge case: current_time_us <= fade_start_us (shouldn't happen)
        final_color = base_color;
      }
    }
    display._set_physical_drumpad_led(i, final_color);
  }
}

Color SequencerDisplayMode::calculate_step_color(
    PizzaDisplay &display, const musin::timing::Step &step) const {
  Color color(0);

  if (step.enabled && step.note.has_value()) {
    std::optional<Color> base_color_opt =
        display.get_color_for_midi_note(step.note.value());

    if (!base_color_opt.has_value()) {
      return Color(0);
    }
    Color base_color = base_color_opt.value();

    uint8_t brightness = PizzaDisplay::MAX_BRIGHTNESS;
    if (step.velocity.has_value()) {
      uint16_t calculated_brightness =
          static_cast<uint16_t>(step.velocity.value()) *
          PizzaDisplay::VELOCITY_TO_BRIGHTNESS_SCALE;
      brightness = static_cast<uint8_t>(
          std::min(calculated_brightness,
                   static_cast<uint16_t>(PizzaDisplay::MAX_BRIGHTNESS)));
    }

    color = Color(display._leds.adjust_color_brightness(
        static_cast<uint32_t>(base_color), brightness));
  }
  return color;
}

Color SequencerDisplayMode::apply_pulsing_highlight(PizzaDisplay &display,
                                                    Color base_color) const {
  uint8_t amount;

  if (display._highlight_is_bright) {
    amount = PizzaDisplay::HIGHLIGHT_BLEND_AMOUNT;
  } else {
    amount = ((PizzaDisplay::HIGHLIGHT_BLEND_AMOUNT *
               PizzaDisplay::REDUCED_BRIGHTNESS) >>
              8);
  }
  return base_color.brighter(amount, PizzaDisplay::MAX_BRIGHTNESS);
}

// --- FileTransferDisplayMode Implementation ---

void FileTransferDisplayMode::on_enter(PizzaDisplay &display) {
  display.clear();
  _chaser_position = 0;
  _last_update_time = nil_time;
}

void FileTransferDisplayMode::draw(PizzaDisplay &display, absolute_time_t now) {
  display.clear();
  // Flash the play button green
  uint32_t time_ms = to_ms_since_boot(now);
  Color pulse_color =
      (time_ms / 250) % 2 == 0 ? PizzaDisplay::COLOR_GREEN : Color(0);
  display.set_play_button_led(pulse_color);
}

// --- BootAnimationMode Implementation ---

BootAnimationMode::BootAnimationMode(
    drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
        &sequencer_controller)
    : _sequencer_controller_ref(sequencer_controller) {
}

void BootAnimationMode::on_enter(PizzaDisplay &display) {
  display.clear();
  _boot_animation_track_index = config::NUM_DRUMPADS - 1;
  _boot_animation_last_step_time = get_absolute_time();
}

void BootAnimationMode::draw(PizzaDisplay &display, absolute_time_t now) {
  const uint32_t animation_step_duration_ms = 400;

  if (absolute_time_diff_us(_boot_animation_last_step_time, now) / 1000 >
      animation_step_duration_ms) {
    _boot_animation_last_step_time = now;

    if (_boot_animation_track_index == 0) {
      display.switch_to_sequencer_mode(); // Transition out of boot mode
      return;
    }

    _boot_animation_track_index--;
  }

  display.clear();
  // Draw sequencer ring for current track
  uint8_t note = _sequencer_controller_ref.get_active_note_for_track(
      _boot_animation_track_index);
  auto ring_color =
      display.get_color_for_midi_note(note).value_or(PizzaDisplay::COLOR_WHITE);

  for (size_t step = 0; step < PizzaDisplay::SEQUENCER_STEPS_DISPLAYED;
       ++step) {
    auto led_index =
        display.get_sequencer_led_index(_boot_animation_track_index, step);
    if (led_index) {
      display._leds.set_pixel(led_index.value(),
                              static_cast<uint32_t>(ring_color));
    }
  }

  // Light up drumpads for tracks that have been "introduced"
  for (uint8_t i = config::NUM_DRUMPADS - 1; i >= _boot_animation_track_index;
       --i) {
    uint8_t pad_note = _sequencer_controller_ref.get_active_note_for_track(i);
    Color pad_color = display.get_color_for_midi_note(pad_note).value_or(
        PizzaDisplay::COLOR_WHITE);
    display._set_physical_drumpad_led(i, pad_color);
    if (i == 0)
      break; // Prevent underflow with uint8_t
  }
}

// --- SleepDisplayMode Implementation ---

void SleepDisplayMode::on_enter(PizzaDisplay &display) {
  _dimming_start_time = get_absolute_time();
  _original_brightness = display.get_brightness();
}

void SleepDisplayMode::set_previous_mode(DisplayMode &previous_mode) {
  _previous_mode = std::reference_wrapper<DisplayMode>(previous_mode);
}

void SleepDisplayMode::draw(PizzaDisplay &display, absolute_time_t now) {
  if (is_nil_time(_dimming_start_time)) {
    return;
  }

  const uint8_t current_brightness = calculate_brightness(now);
  display.set_brightness(current_brightness);

  if (current_brightness > 0 && _previous_mode.has_value()) {
    // Delegate drawing to the previous mode with new brightness
    _previous_mode->get().draw(display, now);
  } else {
    // Dimming complete - clear display
    display.clear();
  }
}

float SleepDisplayMode::apply_ease_out_curve(float progress) const {
  return 1.0f - (1.0f - progress) * (1.0f - progress);
}

uint8_t SleepDisplayMode::calculate_brightness(absolute_time_t now) const {
  const uint64_t current_time_us = to_us_since_boot(now);
  const uint64_t dimming_start_us = to_us_since_boot(_dimming_start_time);
  const uint64_t dimming_duration_us = DIMMING_DURATION_MS * 1000ULL;

  if (current_time_us <= dimming_start_us) {
    return _original_brightness;
  }

  const uint64_t elapsed_us = current_time_us - dimming_start_us;

  if (elapsed_us >= dimming_duration_us) {
    return 0;
  }

  const auto dimming_progress =
      static_cast<float>(elapsed_us) / static_cast<float>(dimming_duration_us);

  const float eased_progress = apply_ease_out_curve(dimming_progress);

  return static_cast<uint8_t>(_original_brightness * (1.0f - eased_progress));
}

} // namespace drum::ui
