#ifndef USB_H_EGTUA9NZ
#define USB_H_EGTUA9NZ

#include <stdint.h>

namespace musin {
namespace usb {

void init();
void disconnect();
bool background_update();
bool midi_read(uint8_t packet[4]);
void midi_send(const uint8_t packet[4]);

} // namespace usb
}; // namespace musin

#endif /* end of include guard: USB_H_EGTUA9NZ */
