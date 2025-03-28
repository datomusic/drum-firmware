#include "usb.h"
#include "device/usbd.h"
#include "tusb.h"

#ifndef USB_DEVICE_INSTANCE
#define USB_DEVICE_INSTANCE 0
#endif

namespace Musin {
namespace Usb {

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
  tud_midi_packet_write(packet);
}

void init() {
  tusb_init();

  while (!tud_cdc_connected()) {
    tud_task();
  }
}

} // namespace Usb
} // namespace Musin
