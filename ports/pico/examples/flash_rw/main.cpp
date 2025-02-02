#include "filesystem/vfs.h"
#include <pico/stdlib.h>
#include <stdio.h>

int main(void) {
  stdio_init_all();

  // Wait for stdio to become available.
  // Not sure why this is needed, but without it messages are dropped.
  sleep_ms(1000);

  printf("Startup\n");
  printf("Initializing fs\n");
  fs_init();

  printf("Opening file\n");
  FILE *fp = fopen("dato_test.txt", "w");

  printf("Writing...\n");
  fprintf(fp, "Rhythm is a dancer!\n");

  printf("Closing file\n");
  fclose(fp);

  printf("Done!\n");
}
