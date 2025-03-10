#include "core/filesystem.h"
#include "core/teensy_audio/mixer.h"
#include "core/timestretched/AudioSampleSnare.h"
#include "file_sound.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include <pico/stdlib.h>
#include <stdio.h>

#define STORE_SAMPLE 0
#define REFORMAT false

// Path must start with backslash in order to be valid under the root mount
// point.
static const char *file_name = "/snare_sample";

static const uint master_volume = 10;
FileSound sound;
BufferSource *sounds[1] = {&sound};
AudioMixer4 mixer(sounds, 1);

static void store_sample() {
  printf("Opening file for writing\n");
  FILE *fp = fopen(file_name, "wb");

  if (!fp) {
    printf("Error: Write open failed\n");
    return;
  }

  auto written = fwrite(AudioSampleSnare, sizeof(AudioSampleSnare[0]),
                        AudioSampleSnareSize, fp);
  printf("Wrote %i bytes\n", written);
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

static void __not_in_flash_func(fill_audio_buffer)(audio_buffer_pool_t *pool) {
  audio_buffer_t *out_buffer = take_audio_buffer(pool, false);
  if (out_buffer == NULL) {
    // printf("Failed to take audio buffer\n");
    return;
  }

  static int16_t temp_samples[AUDIO_BLOCK_SAMPLES];
  mixer.fill_buffer(temp_samples);

  // Convert to 32bit stereo
  int32_t *stereo_out_samples = (int32_t *)out_buffer->buffer->bytes;
  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
    int32_t sample = (master_volume * temp_samples[i]) << 8u;
    sample = sample + (sample >> 16u);
    stereo_out_samples[i * 2] = sample;
    stereo_out_samples[i * 2 + 1] = sample;
  }

  out_buffer->sample_count = AUDIO_BLOCK_SAMPLES;
  give_audio_buffer(pool, out_buffer);
}

static bool init() {
  init_clock();
  stdio_init_all();
  // Give host some time to catch up, otherwise messages can be lost.
  sleep_ms(2000);

  printf("Startup\n");
  printf("\n\n");
  printf("Initializing fs\n");
  const auto init_result = init_filesystem(REFORMAT);
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

#if STORE_SAMPLE
  store_sample();
#endif

  printf("Opening for reading\n");

  FILE *fp = fopen(file_name, "rb");
  if (fp) {
    printf("Reading\n");
    if (fseek(fp, 0, SEEK_END) != 0) {
      printf("Seek failed!\n");
    }

    const auto size = ftell(fp);
    printf("size: %li\n", size);
    fclose(fp);
    printf("File closed!\n");
  } else {
    printf("Error: Read open failed\n");
  }

  sound.load(file_name);

  printf("Initializing audio output\n");
  AudioOutput::init(fill_audio_buffer);

  printf("Entering loop!\n");
  int counter = 0;
  while (true) {
    if (++counter > 30000000) {
      printf("Playing sample\n");
      counter = 0;
      sound.play(1.0);
    }

    if (sound.reader.needs_update) {
      /*printf("Updating sample\n");*/
      sound.reader.update();
    }
  }
}
