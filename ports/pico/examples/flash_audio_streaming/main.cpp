#include "core/timestretched/AudioSampleSnare.h"
#include "core/filesystem.h"
#include <pico/stdlib.h>
#include <stdio.h>

#define STORE_SAMPLE 0
#define REFORMAT false

// Path must start with backslash in order to be valid under the root mount
// point.
static const char *file_name = "/snare_sample";

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

static bool init() {
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
  } else {
    printf("Error: Read open failed\n");
  }

  printf("Done!\n");
  while (true) {
    sleep_ms(1);
  }
}
