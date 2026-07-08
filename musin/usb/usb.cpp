#include "usb.h"
#include "class/midi/midi_device.h"
#include "device/usbd.h"
#include "tusb.h"

extern "C" {
#include "pico/time.h"
}

#ifndef USB_DEVICE_INSTANCE
#define USB_DEVICE_INSTANCE 0
#endif

namespace musin {
namespace usb {

bool background_update(void) {
  if (tusb_inited()) {
    tud_task();
    return true;
  } else {
    return false;
  }
}

void disconnect() {
  tud_disconnect();
}

bool midi_read(uint8_t packet[4]) {
  bool ret = false;
  if (tud_midi_available()) {
    tud_midi_packet_read(packet);
    ret = true;
  }

  return ret;
}

void midi_send(const uint8_t packet[4]) {
  // A long SysEx message (e.g. a 127-byte SDS data packet) exceeds the
  // 64-byte MIDI TX FIFO. When the FIFO is full, drain USB and retry instead
  // of silently dropping the rest of the message; give up after a deadline
  // so an unresponsive host cannot stall the main loop.
  const absolute_time_t deadline = make_timeout_time_ms(10);
  while (!tud_midi_packet_write(packet)) {
    if (!tud_ready() || time_reached(deadline)) {
      return;
    }
    tud_task();
  }
}

void init(const bool block_until_connected) {
  tusb_init();

  if (block_until_connected) {
    while (!tud_cdc_connected()) {
      tud_task();
    }
  }
}

} // namespace usb
} // namespace musin
