#include <cstdint>

#include "musin/usb/usb.h"

#include "pico/stdlib.h" // for stdio_usb_init
#include "pico/time.h"   // for sleep_us

#include "internal_clock.h"
#include "midi.h"
#include "pizza_controls.h"
#include "pizza_display.h"
#include "sequencer_controller.h"
#include "step_sequencer.h"
#include "tempo_handler.h"
#include "tempo_multiplier.h"

static PizzaExample::PizzaDisplay pizza_display;
static StepSequencer::Sequencer<4, 8> pizza_sequencer;
static Clock::InternalClock internal_clock(120.0f);

static PizzaControls pizza_controls(pizza_display, pizza_sequencer, internal_clock);

static Tempo::TempoHandler tempo_handler(Tempo::ClockSource::INTERNAL);
static Tempo::TempoMultiplier tempo_multiplier(1, 1);

StepSequencer::SequencerController sequencer_controller(pizza_sequencer, tempo_multiplier);
// TODO: Instantiate MIDIClock, ExternalSyncClock when available

int main() {
  stdio_usb_init();

  Musin::Usb::init();

  midi_init();

  pizza_display.init();

  pizza_controls.init();

  // --- Initialize Clocking System ---
  internal_clock.add_observer(tempo_handler);
  tempo_handler.add_observer(tempo_multiplier);
  tempo_multiplier.add_observer(sequencer_controller);

  if (tempo_handler.get_clock_source() == Tempo::ClockSource::INTERNAL) {
    internal_clock.start();
  }
  // TODO: Add logic to register TempoHandler with other clocks
  // TODO: Add logic to start/stop clocks based on TempoHandler source selection

  pizza_sequencer.get_track(0).get_step(0) = {36, 100, true};
  pizza_sequencer.get_track(0).get_step(4) = {36, 100, true};
  pizza_sequencer.get_track(1).get_step(2) = {38, 100, true};
  pizza_sequencer.get_track(1).get_step(6) = {38, 100, true};

  while (true) {
    pizza_controls.update();

    pizza_display.draw_sequencer_state(pizza_sequencer, sequencer_controller.get_current_step());

    pizza_display.show();
    Musin::Usb::background_update();
    midi_read();

    // Brief Delay: Important for WS2812 LED latching after show()
    sleep_us(80);
  }

  return 0;
}
