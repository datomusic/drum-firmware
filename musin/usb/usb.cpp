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

namespace {

// USB-MIDI code index numbers carrying SysEx: start/continue (0x4) and the
// three end forms (0x5..0x7). Single-byte real-time messages use CIN 0xF.
bool is_sysex_packet(const uint8_t packet[4]) {
  const uint8_t cin = packet[0] & 0x0Fu;
  return cin >= 0x4u && cin <= 0x7u;
}

} // namespace

void midi_send(const uint8_t packet[4]) {
  // A long SysEx message (e.g. a 127-byte SDS data packet) expands to far more
  // than the 64-byte MIDI TX FIFO holds, and the FIFO only regains space once
  // tud_task() processes an IN-transfer completion. Without yielding here the
  // tail of the message is silently dropped and the host never sees the F7, so
  // SysEx retries until the FIFO accepts the packet.
  //
  // Every other message is a single packet. Retrying one of those cannot help:
  // a full FIFO means the host is not draining the endpoint at all, so the loop
  // always burns its full deadline. That stalled the main loop by up to 10 ms
  // per outgoing note, which showed up as sluggish pot response and late steps
  // whenever the device was attached to a host that had not opened the port.
  // Dropping a stale channel message is the cheaper failure.
  if (!is_sysex_packet(packet)) {
    tud_midi_packet_write(packet);
    return;
  }

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
