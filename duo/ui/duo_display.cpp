#include "duo/ui/duo_display.h"

#include "etl/array.h"

extern "C" {
#include "hardware/gpio.h"
}

namespace {

// DRUM physical LED indices (drum_pizza_hardware.h)
constexpr uint32_t LED_PLAY_BUTTON = 0;
constexpr uint32_t LED_DRUMPAD_3 = 23; // DUO env / peak meter
constexpr uint32_t LED_DRUMPAD_4 = 32; // DUO filter LED
constexpr etl::array<uint32_t, 32> LED_ARRAY = {
    1,  2,  3,  4,  6,  7,  8,  9,  10, 11, 12, 13, 15, 16, 17, 18,
    19, 20, 21, 22, 24, 25, 26, 27, 28, 29, 30, 31, 33, 34, 35, 36};
constexpr size_t TRACKS = 4;

// Physical LED index for a (track, step) button, matching the keypad mapping:
// LED_ARRAY[step * 4 + (3 - track)].
constexpr uint32_t track_step_led(uint8_t track, uint8_t step) {
  return LED_ARRAY[step * TRACKS + (TRACKS - 1 - track)];
}

constexpr uint32_t LED_ENABLE_PIN = DATO_SUBMARINE_LED_ENABLE_PIN;
constexpr uint32_t LED_DATA_PIN = PICO_DEFAULT_WS2812_PIN;

// DUO Leds.h constants
constexpr uint8_t SK6812_BRIGHTNESS = 32;
constexpr uint32_t CORRECTION_SK6812 = 0xFFF1E0;
constexpr uint32_t LED_WHITE = (230u << 16) | (255u << 8) | 150u;
constexpr uint32_t LED_BLACK = 0;

// The black keys have assigned colors. The white keys are shown in gray.
constexpr uint32_t COLORS[] = {0x444444, 0xFF0001, 0x444444, 0xFFDD00, 0x444444,
                               0x444444, 0x11FF00, 0x444444, 0x0033DD, 0x444444,
                               0xFF00FF, 0x444444, 0x444444, 0xFF2209, 0x444444,
                               0x99FF00, 0x444444, 0x444444, 0x00EE22, 0x444444,
                               0x0099CC, 0x444444, 0xBB33BB, 0x444444};

// DUO keyboard scale, for key colors (globals.h)
constexpr uint8_t SCALE[10] = {49, 51, 54, 56, 58, 61, 63, 66, 68, 70};

// --- FastLED-equivalent helpers (color math only, ~CRGB semantics) ---

// FastLED fadeLightBy(x): scale channels by (255 - x) / 256.
constexpr uint32_t fade_light_by(uint32_t color, uint8_t fade) {
  const uint32_t scale = 255 - fade;
  const uint32_t r = (((color >> 16) & 0xFF) * scale) >> 8;
  const uint32_t g = (((color >> 8) & 0xFF) * scale) >> 8;
  const uint32_t b = ((color & 0xFF) * scale) >> 8;
  return (r << 16) | (g << 8) | b;
}

// FastLED blend(a, b, amount): linear interpolation a -> b by amount/255.
constexpr uint32_t blend(uint32_t a, uint32_t b, uint8_t amount) {
  const int32_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
  const int32_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
  const uint32_t r = ar + (((br - ar) * amount) >> 8);
  const uint32_t g = ag + (((bg - ag) * amount) >> 8);
  const uint32_t bl = ab + (((bb - ab) * amount) >> 8);
  return (r << 16) | (g << 8) | bl;
}

// Scale a color to a level in [0.0, 1.0] (replaces the DUO's PWM panel LEDs).
constexpr uint32_t scale_color(uint32_t color, float level) {
  if (level < 0.0f) {
    level = 0.0f;
  } else if (level > 1.0f) {
    level = 1.0f;
  }
  return fade_light_by(color, 255 - static_cast<uint8_t>(level * 255.0f));
}

} // namespace

namespace duo::ui {

DuoDisplay::DuoDisplay()
    : leds_(LED_DATA_PIN, musin::drivers::RGBOrder::GRB, SK6812_BRIGHTNESS,
            CORRECTION_SK6812) {
}

bool DuoDisplay::init() {
  if (!leds_.init()) {
    return false;
  }
  gpio_init(LED_ENABLE_PIN);
  gpio_set_dir(LED_ENABLE_PIN, GPIO_OUT);
  gpio_put(LED_ENABLE_PIN, 1);
  leds_.clear();
  leds_.show();
  return true;
}

void DuoDisplay::deinit() {
  gpio_put(LED_ENABLE_PIN, 0);
}

void DuoDisplay::set_step_led(uint8_t step, uint32_t color) {
  leds_.set_pixel(track_step_led(3, step % Sequencer::NUM_STEPS), color);
}

void DuoDisplay::draw_keyboard() {
  // KEYB_0..7 on the Track 1 (outer) row, KEYB_8/9 on Track 2 steps 0/1.
  for (uint8_t i = 0; i < 8; ++i) {
    leds_.set_pixel(track_step_led(0, i), COLORS[SCALE[i] % 24]);
  }
  leds_.set_pixel(track_step_led(1, 0), COLORS[SCALE[8] % 24]);
  leds_.set_pixel(track_step_led(1, 1), COLORS[SCALE[9] % 24]);
}

void DuoDisplay::draw_steps(const Sequencer::Sequencer &sequencer) {
  for (uint8_t l = 0; l < Sequencer::NUM_STEPS; ++l) {
    if (sequencer.get_step_enabled(l)) {
      set_step_led(l, COLORS[sequencer.get_step_note(l) % 24]);
    } else {
      set_step_led(l, LED_BLACK);
    }
  }
}

// DUO Leds.h led_update() transport section, near-verbatim.
void DuoDisplay::draw_transport(const Sequencer::Sequencer &sequencer) {
  const auto cur_seq_step = sequencer.cur_step_index();

  if (sequencer.gate_active()) {
    set_step_led(cur_seq_step, LED_WHITE);
  }

  if (sequencer.is_running()) {
    leds_.set_pixel(LED_PLAY_BUTTON, LED_WHITE);
  } else {
    if (sequencer.note_playing()) {
      set_step_led(Sequencer::wrapped_step(cur_seq_step), LED_WHITE);
    } else {
      const unsigned step_ticks = Sequencer::TICKS_PER_STEP;
      const uint32_t seq_clock = sequencer.get_clock() + step_ticks;
      const uint8_t fade_val = (seq_clock % step_ticks) * 16;
      const bool fade_play = (seq_clock % (2 * step_ticks)) < step_ticks;

      // Toggle between fading play button or current step.
      if (fade_play) {
        leds_.set_pixel(LED_PLAY_BUTTON, fade_light_by(LED_WHITE, fade_val));
      } else {
        leds_.set_pixel(LED_PLAY_BUTTON, LED_BLACK);

        if (sequencer.get_step_enabled(cur_seq_step)) {
          set_step_led(cur_seq_step,
                       blend(LED_WHITE,
                             COLORS[sequencer.get_step_note(cur_seq_step) % 24],
                             fade_val));
        } else {
          set_step_led(cur_seq_step, fade_light_by(LED_WHITE, fade_val));
        }
      }
    }
  }
}

void DuoDisplay::draw_meters(const synth_parameters &synth, float peak_level) {
  // DUO env LED (LED3) -> drumpad LED 3; filter LED -> drumpad LED 4.
  leds_.set_pixel(LED_DRUMPAD_3, scale_color(LED_WHITE, peak_level));
  leds_.set_pixel(LED_DRUMPAD_4, scale_color(0xFFDD00, synth.filter / 1023.0f));
}

void DuoDisplay::update(const Sequencer::Sequencer &sequencer,
                        const synth_parameters &synth, float peak_level) {
  leds_.clear();
  draw_keyboard();
  draw_steps(sequencer);
  draw_transport(sequencer);
  draw_meters(synth, peak_level);
  leds_.show();
}

} // namespace duo::ui
