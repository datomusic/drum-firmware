#include "drum/ui/pizza_display.h"
#include "drum/drum_pizza_hardware.h"
#include "drum/ui/display_mode.h"
#include "musin/timing/timing_constants.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace drum {

namespace {

constexpr uint32_t DEFAULT_COLOR_CORRECTION = 0xffe080;

} // anonymous namespace

PizzaDisplay::PizzaDisplay(
    drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
        &sequencer_controller_ref,
    musin::timing::TempoHandler &tempo_handler_ref, musin::Logger &logger_ref)
    : _leds(PIZZA_LED_DATA_PIN, musin::drivers::RGBOrder::GRB, MAX_BRIGHTNESS,
            DEFAULT_COLOR_CORRECTION),
      _drumpad_fade_start_times{}, _logger_ref(logger_ref),
      sequencer_mode_(sequencer_controller_ref, tempo_handler_ref),
      transfer_mode_(), boot_animation_mode_(sequencer_controller_ref) {
  for (size_t i = 0; i < config::NUM_DRUMPADS; ++i) {
    _drumpad_fade_start_times[i] = nil_time;
  }
  for (size_t i = 0; i < SEQUENCER_TRACKS_DISPLAYED; ++i) {
    _track_override_colors[i] = std::nullopt;
  }
  current_mode_ = &sequencer_mode_;
}

void PizzaDisplay::notification(musin::timing::TempoEvent event) {
  // Update highlight state based on downbeat and eighth offbeat for blinking
  if (event.phase_24 == musin::timing::PHASE_DOWNBEAT ||
      event.phase_24 == musin::timing::PHASE_EIGHTH_OFFBEAT) {
    bool prev = _highlight_is_bright.load(std::memory_order_relaxed);
    _highlight_is_bright.store(!prev, std::memory_order_relaxed);
  }
}

void PizzaDisplay::notification(
    drum::Events::SysExTransferStateChangeEvent event) {
  // Update file transfer mode with current sample slot if provided
  if (event.current_sample_slot.has_value()) {
    transfer_mode_.set_current_sample_slot(event.current_sample_slot.value());
  }
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

std::optional<Color>
PizzaDisplay::get_color_for_midi_note(uint8_t midi_note_number) const {
  for (const auto &note_def : config::global_note_definitions) {
    if (note_def.midi_note_number == midi_note_number) {
      return Color(note_def.color);
    }
  }
  return std::nullopt; // MIDI note not found in global definitions
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

void PizzaDisplay::deinit() {
  gpio_put(PIZZA_LED_ENABLE_PIN, 0);
}

void PizzaDisplay::update(absolute_time_t now) {
  if (current_mode_) {
    current_mode_->draw(*this, now);
  }
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

uint8_t PizzaDisplay::get_brightness() const {
  return _leds.get_brightness();
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

void PizzaDisplay::start_boot_animation() {
  current_mode_ = &boot_animation_mode_;
  current_mode_->on_enter(*this);
}

void PizzaDisplay::switch_to_sequencer_mode() {
  // Check if we're transitioning from boot animation
  bool transitioning_from_boot = (current_mode_ == &boot_animation_mode_);

  current_mode_ = &sequencer_mode_;
  current_mode_->on_enter(*this);

  // If this transition is from boot animation, notify the callback
  if (transitioning_from_boot && boot_complete_callback_) {
    boot_complete_callback_();
  }
}

void PizzaDisplay::switch_to_file_transfer_mode() {
  current_mode_ = &transfer_mode_;
  current_mode_->on_enter(*this);
}

void PizzaDisplay::start_sleep_mode() {
  // Capture the current mode as the previous mode before switching
  if (current_mode_ != nullptr) {
    sleep_mode_.set_previous_mode(*current_mode_);
  }
  current_mode_ = &sleep_mode_;
  current_mode_->on_enter(*this);
}

void PizzaDisplay::set_boot_complete_callback(std::function<void()> callback) {
  boot_complete_callback_ = callback;
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
