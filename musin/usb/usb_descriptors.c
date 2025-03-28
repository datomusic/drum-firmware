// /*
//  * The MIT License (MIT)
//  *
//  * Copyright (c) 2019 Ha Thach (tinyusb.org)
//  *
//  * Permission is hereby granted, free of charge, to any person obtaining a
//  copy
//  * of this software and associated documentation files (the "Software"), to
//  deal
//  * in the Software without restriction, including without limitation the
//  rights
//  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  * copies of the Software, and to permit persons to whom the Software is
//  * furnished to do so, subject to the following conditions:
//  *
//  * The above copyright notice and this permission notice shall be included in
//  * all copies or substantial portions of the Software.
//  *
//  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE
//  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM,
//  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  * THE SOFTWARE.
//  *
//  */

#include "tusb.h"
#include "pico/unique_id.h"

/* Define Product ID with CDC and MIDI enabled */
#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))
#define USB_PID (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MIDI, 3))


#define USBD_STR_0 (0x00)
#define USBD_STR_MANUF (0x01)
#define USBD_STR_PRODUCT (0x02)
#define USBD_STR_SERIAL (0x03)
#define USBD_STR_CDC (0x04)
#define USBD_STR_RPI_RESET (0x05)

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {.bLength = sizeof(tusb_desc_device_t),
                                        .bDescriptorType = TUSB_DESC_DEVICE,
                                        .bcdUSB = 0x0200,
                                        .bDeviceClass = 0x00,
                                        .bDeviceSubClass = 0x00,
                                        .bDeviceProtocol = 0x00,
                                        .bMaxPacketSize0 =
                                            CFG_TUD_ENDPOINT0_SIZE,

                                        .idVendor = 0xCafe,
                                        .idProduct = USB_PID,
                                        .bcdDevice = 0x0100,

                                        .iManufacturer = USBD_STR_MANUF,
                                        .iProduct = USBD_STR_PRODUCT,
                                        .iSerialNumber = USBD_STR_SERIAL,

                                        .bNumConfigurations = 0x01};

// Invoked when received GET DEVICE DESCRIPTOR
uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum {
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
  ITF_NUM_MIDI,
  ITF_NUM_MIDI_STREAMING,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN                                                       \
  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MIDI_DESC_LEN)

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82
#define USBD_CDC_CMD_MAX_SIZE (8)
#define EPNUM_MIDI_OUT 0x03
#define EPNUM_MIDI_IN 0x83

uint8_t const desc_fs_configuration[] = {
    // Config number, interface count, string index, total length, attribute,
    // power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // CDC: Interface number, string index, EP notification, EP OUT & IN
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, USBD_STR_CDC, EPNUM_CDC_NOTIF, USBD_CDC_CMD_MAX_SIZE, EPNUM_CDC_OUT,
                       EPNUM_CDC_IN, 64),

    // MIDI: Interface number, string index, EP OUT & EP IN
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, 64)};

#if TUD_OPT_HIGH_SPEED
uint8_t const desc_hs_configuration[] = {
    // Config number, interface count, string index, total length, attribute,
    // power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // CDC: Interface number, string index, EP notification, EP OUT & IN
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, USBD_CDC_CMD_MAX_SIZE, EPNUM_CDC_OUT,
                       EPNUM_CDC_IN, 512),

    // MIDI: Interface number, string index, EP OUT & EP IN
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, 512)};
#endif

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;

#if TUD_OPT_HIGH_SPEED
  return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration
                                              : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

static char usbd_serial_str[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];

char const *usbd_desc_str[] = {
    [USBD_STR_MANUF] = "DATO",
    [USBD_STR_PRODUCT] = "Drum",
    [USBD_STR_SERIAL] = "000001",
    [USBD_STR_CDC] = "Board CDC",
#if PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE
    [USBD_STR_RPI_RESET] = "Reset",
#endif
};

static uint16_t _desc_str[32];

const uint16_t *tud_descriptor_string_cb(uint8_t index, __unused uint16_t langid) {
#ifndef USBD_DESC_STR_MAX
#define USBD_DESC_STR_MAX (20)
#elif USBD_DESC_STR_MAX > 127
#error USBD_DESC_STR_MAX too high (max is 127).
#elif USBD_DESC_STR_MAX < 17
#error USBD_DESC_STR_MAX too low (min is 17).
#endif
    static uint16_t desc_str[USBD_DESC_STR_MAX];

    // Assign the SN using the unique flash id
    if (!usbd_serial_str[0]) {
        pico_get_unique_board_id_string(usbd_serial_str, sizeof(usbd_serial_str));
    }

    uint8_t len;
    if (index == 0) {
        desc_str[1] = 0x0409; // supported language is English
        len = 1;
    } else {
        if (index >= sizeof(usbd_desc_str) / sizeof(usbd_desc_str[0])) {
            return NULL;
        }
        const char *str = usbd_desc_str[index];
        for (len = 0; len < USBD_DESC_STR_MAX - 1 && str[len]; ++len) {
            desc_str[1 + len] = str[len];
        }
    }

    // first byte is length (including header), second byte is string type
    desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * len + 2));

    return desc_str;
}
