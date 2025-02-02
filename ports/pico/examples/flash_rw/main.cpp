#include "filesystem/vfs.h"
#include <pico/stdlib.h>
#include <stdio.h>

int main(void) {
  stdio_init_all();
  const auto init_result = fs_init();

  // Give host some time to catch up, otherwise messages can be lost.
  sleep_ms(1000);

  printf("Startup\n");
  printf("Initializing fs\n");
  if (init_result) {
    printf("fs initialized\n");
    printf("Opening file\n");
    FILE *fp = fopen("/DATO.TXT", "w");

    printf("Writing...\n");
    fprintf(fp, "Rhythm is a dancer!\n");

    printf("Closing file\n");
    fclose(fp);
    printf("Opening for reading\n");
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
