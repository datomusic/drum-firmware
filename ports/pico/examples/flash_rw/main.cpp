#include <pico/stdlib.h>
#include <stdio.h>
#include "core/filesystem.h"

int main(void) {
  stdio_init_all();
  printf("Startup\n");

  sleep_ms(1000);

  // Give host some time to catch up, otherwise messages can be lost.

  printf("\n\n");
  printf("Initializing fs\n");
  const auto init_result = init_filesystem(true);
  if (init_result) {
    printf("fs initialized\n");
    printf("Opening file\n");
    FILE *fp = fopen("/DATO.TXT", "w");

    printf("Writing...\n");
    fprintf(fp, "Rhythm is a dancer!\n");

    printf("Closing file\n");
    fclose(fp);
    printf("Opening for reading\n");

    // Path must start with backslash, otherwise writing freezes (or crashes?).
    fp = fopen("/DATO.TXT", "r");

    printf("Reading\n");
    char buffer[128] = {0};
    fgets(buffer, sizeof(buffer), fp);

    printf("Closing file\n");
    fclose(fp);

    printf("content: %s\n", buffer);
  } else {
    printf("Initialization failed: %i\n", init_result);
  }

  printf("Done!\n");
  while (true) {
    sleep_ms(1);
  }
}
