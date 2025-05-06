#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include <stdio.h>

#include "bsp/board.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/usb/usb.h"
#include "tusb.h"
#include <math.h>
#include <stdio.h>

#include "pico/bootrom.h"

#define SYSEX_DATO_ID 0x7D
#define SYSEX_DUO_ID 0x64
#define SYSEX_REBOOT_BOOTLOADER 0x0B

static void enter_bootloader() {
  reset_usb_boot(0, 0);
}

static void handle_sysex(byte *const data, const unsigned length) {
  if (data[1] == SYSEX_DATO_ID && data[2] == SYSEX_DUO_ID && data[3] == SYSEX_REBOOT_BOOTLOADER) {
    enter_bootloader();
  }
}

void handle_note_on(byte channel, byte note, byte velocity) {
}

void handle_note_off(byte channel, byte note, byte velocity) {
}

void led_blinking_task(void);

int main() {
  // stdio_init_all();
  board_init();
  musin::usb::init();

  MIDI::init(MIDI::Callbacks{
      .note_on = handle_note_on, .note_off = handle_note_off, .sysex = handle_sysex});

  static uint32_t last_ms = board_millis();
  while (1) {
    musin::usb::background_update();
    MIDI::read(1);
    led_blinking_task();
    const uint32_t now_ms = board_millis();
    if (now_ms - last_ms > 1000) {
      last_ms = now_ms;
      MIDI::sendNoteOn(70, 127, 1);
    }
  }

  return 0;
}

enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
const uint LED_PIN = PICO_DEFAULT_LED_PIN;

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void)remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

void led_blinking_task(void) {
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if (board_millis() - start_ms < blink_interval_ms)
    return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
