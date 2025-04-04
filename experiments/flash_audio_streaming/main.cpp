#include "file_sound.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/sync.h"
#include "hardware/structs/clocks.h"
#include "musin/drivers/aic3204.h"
#include "musin/audio/mixer.h"
#include "musin/filesystem/filesystem.h"
#include "musin/midi/midi_wrapper.h"
#include "samples/AudioSampleGong.h"
#include "samples/AudioSampleHihat.h"
#include "samples/AudioSampleKick.h"
#include "samples/AudioSampleSnare.h"
#include "tusb.h"
#include "musin/usb/usb.h"
#include <pico/stdlib.h>
#include <stdio.h>

#define STORE_SAMPLES false
#define REFORMAT false

#define MIDI_CHANNEL 1

// Path must start with backslash in order to be valid
// under the root mount point.

static const uint master_volume = 10;
FileSound snare;
FileSound hihat;
FileSound kick;
FileSound gong;
/*
FileSound *sounds[4] = {&snare, &kick, &hihat, &gong};
AudioMixer4 mixer((BufferSource **)sounds, 4);
*/

#define SAMPLE_COUNT 4
FileSound *sounds[SAMPLE_COUNT] = {&hihat, &snare, &kick, &gong};
AudioMixer4 mixer((BufferSource **)sounds, SAMPLE_COUNT);

static void store_sample(const char *file_name, const unsigned int *sample_data,
                         const uint32_t data_length) {
  printf("Opening file for writing\n");
  FILE *fp = fopen(file_name, "wb");

  if (!fp) {
    printf("Error: Write open failed\n");
    return;
  }

  AudioMemoryReader reader(sample_data, data_length);
  reader.reset();

  int16_t buffer[AUDIO_BLOCK_SAMPLES];

  int written = 0;
  while (reader.has_data()) {
    const auto sample_count = reader.read_samples(buffer);
    written += fwrite(buffer, sizeof(buffer[0]), sample_count, fp);
  }

  printf("Wrote %i samples\n", written);
  printf("Closing file\n");
  fclose(fp);
}

static void init_clock() {
  // Set PLL_USB 96MHz
  pll_init(pll_usb, 1, 1536 * MHZ, 4, 4);
  clock_configure(clk_usb, 0, CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                  96 * MHZ, 48 * MHZ);
  // Change clk_sys to be 96MHz.
  clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                  CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, 96 * MHZ,
                  96 * MHZ);
  // CLK peri is clocked from clk_sys so need to change clk_peri's freq
  clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                  96 * MHZ, 96 * MHZ);
}

static void __not_in_flash_func(fill_audio_buffer)(audio_buffer_t *out_buffer) {
  // printf("Filling buffer\n");

  static int16_t temp_samples[AUDIO_BLOCK_SAMPLES];
  mixer.fill_buffer(temp_samples);

  // Convert to 32bit stereo
  int16_t *stereo_out_samples = (int16_t *)out_buffer->buffer->bytes;
  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
    stereo_out_samples[i] = (master_volume * temp_samples[i]) >> 8u;
  }

  out_buffer->sample_count = AUDIO_BLOCK_SAMPLES;
}

static void handle_sysex(byte *const, const unsigned) {
}

void handle_cc(byte channel, byte controller, byte value) {
    if (controller == 7) { // MIDI Volume Control (CC7)
        // Direct mapping: MIDI 0-127 to -127-0 (0dB at max)
        int8_t volume = value - 127;
        aic3204_dac_set_volume(volume);
        printf("Set volume to %d (CC7 value: %d)\n", volume, value);
    }
}

void handle_note_on(byte, byte note, byte velocity) {
  printf("Received midi note %d\n", note);
  const float pitch = (float)(velocity) / 64.0;
  switch ((note - 1) % 4) {
  case 0:
    kick.play(pitch);
    break;
  case 1:
    snare.play(pitch);
    break;
  case 2:
    hihat.play(pitch);
    break;
  case 3:
    gong.play(pitch);
    break;
  }
}

void handle_note_off(byte, byte, byte) {
}

static bool init() {
  stdio_init_all();
  Musin::Usb::init();
  MIDI::init(MIDI::Callbacks{
      .note_on = handle_note_on,
      .note_off = handle_note_off,
      .clock = nullptr,
      .start = nullptr,
      .cont = nullptr,
      .stop = nullptr,
      .cc = handle_cc,
      .sysex = handle_sysex,
  });
  init_clock();
  // Give host some time to catch up, otherwise messages can be lost.
  sleep_ms(2000);

#ifdef DATO_SUBMARINE
  // Initialize AIC3204 codec with I2C0 pins (GP0=SDA, GP1=SCL) at 400kHz
  if (!aic3204_init(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, 100'000U)) {
    printf("Failed to initialize AIC3204 codec\n");
    return false;
  }
  // Set initial volume to 0dB (max)
  aic3204_dac_set_volume(0);
#endif

  printf("Startup\n");
  printf("\n\n");
  printf("Initializing fs\n");
  const auto init_result = Musin::Filesystem::init(REFORMAT);
  if (!init_result) {
    printf("Initialization failed: %i\n", init_result);
    return false;
  }

  printf("file system initialized\n");
  return true;
}

int main(void) {
  if (!init()) {
    printf("Init failed!\n");
    return 1;
  }

#if STORE_SAMPLES
  store_sample("/snare", AudioSampleSnare, AudioSampleSnareSize);
  store_sample("/kick", AudioSampleKick, AudioSampleKickSize);
  store_sample("/hihat", AudioSampleHihat, AudioSampleHihatSize);
  store_sample("/gong", AudioSampleGong, AudioSampleGongSize);
#endif
  snare.load("/snare");
  hihat.load("/hihat");
  kick.load("/kick");
  gong.load("/gong");

  printf("Initializing audio output\n");
  AudioOutput::init();

  printf("Entering loop!\n");

  while (true) {
    AudioOutput::update(fill_audio_buffer);
    Musin::Usb::background_update();
    MIDI::read(MIDI_CHANNEL);
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
      FileSound *sound = sounds[i];
      if (sound->reader.needs_update) {
        const auto status = save_and_disable_interrupts();
        sound->reader.update();
        restore_interrupts(status);
      }
    }
  }
}
