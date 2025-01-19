#include "usb.h"
#include "device/usbd.h"
#include "tusb.h"

#ifndef USB_DEVICE_INSTANCE
#define USB_DEVICE_INSTANCE 0
#endif

bool DatoUSB::background_update(void) {
  if (tusb_inited()) {
    tud_task();
    return true;
  } else {
    return false;
  }
}

void DatoUSB::disconnect() {
  tud_disconnect();
}

bool DatoUSB::midi_read(uint8_t packet[4]) {
  bool ret = false;
  if (tud_midi_available()) {
    tud_midi_packet_read(packet);
    ret = true;
  }

  return ret;
}

void DatoUSB::midi_send(const uint8_t packet[4]) {
  tud_midi_packet_write(packet);
}

void DatoUSB::init() {
  tusb_init();
}
