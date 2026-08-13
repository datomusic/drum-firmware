#ifndef DUO_UI_DUO_DISPLAY_H
#define DUO_UI_DUO_DISPLAY_H

#include "duo/seq.h"
#include "duo/synth_params.h"
#include "musin/drivers/ws2812-dma.h"
#include <cstdint>

namespace duo::ui {

/**
 * @brief DUO LED behavior (duo-imxrt Leds.h) re-indexed onto the DRUM's
 * 37-LED surface, on musin's WS2812 driver.
 *
 * Layout (§6.3, driven by the §6.1 control mapping):
 * - Keyboard KEYB_0..7 colors on the Track 1 (outer) row, KEYB_8/9 on the
 *   first two Track 2 LEDs.
 * - Sequencer steps on the Track 4 (inner) row, colored by recorded note.
 * - Play button LED, including the DUO's tempo-synced stopped animation.
 * - Peak meter (DUO env/LED3) on drumpad LED 3, filter amount on drumpad
 *   LED 4 (DUO's osc LED is not replicated).
 */
class DuoDisplay {
public:
  DuoDisplay();

  bool init();
  void deinit();

  /**
   * @brief Redraws all LEDs from the current sequencer/synth state.
   * @param peak_level Output peak in [0.0, 1.0] (DUO env LED / LED3 meter).
   */
  void update(const Sequencer::Sequencer &sequencer,
              const synth_parameters &synth, float peak_level);

private:
  static constexpr uint32_t NUM_LEDS = 37;

  void draw_keyboard();
  void draw_steps(const Sequencer::Sequencer &sequencer);
  void draw_transport(const Sequencer::Sequencer &sequencer);
  void draw_meters(const synth_parameters &synth, float peak_level);

  void set_step_led(uint8_t step, uint32_t color);

  musin::drivers::WS2812_DMA<NUM_LEDS> leds_;
};

} // namespace duo::ui

#endif // DUO_UI_DUO_DISPLAY_H
