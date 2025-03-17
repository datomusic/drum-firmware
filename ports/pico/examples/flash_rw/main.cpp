#include "core/filesystem.h"
#include <pico/stdlib.h>
#include <stdio.h>

int main(void) {
  stdio_init_all();
  // Give host some time to catch up, otherwise messages can be lost.
  sleep_ms(2000);

  printf("Startup\n");

  printf("\n\n");
  printf("Initializing fs\n");
  const auto init_result = init_filesystem(true);
  if (init_result) {
    printf("fs initialized\n");
    printf("Opening file for writing\n");

    // Path must start with backslash in order to be valid under the root mount
    // point.
    FILE *fp = fopen("/DATO.TXT", "w");

    if (fp) {
      printf("Writing...\n");
      fprintf(fp, "Rhythm is a flash_rw!\n");
      printf("Closing file\n");
      fclose(fp);
    } else {
      printf("Error: Failed opening for writing\n");
    }

    printf("Opening for reading\n");

    // Path must start with backslash, otherwise writing freezes (or crashes?).
    fp = fopen("/DATO.TXT", "r");
    if (fp) {
      printf("Reading\n");
      char buffer[128] = {0};
      fgets(buffer, sizeof(buffer), fp);

      printf("Closing file\n");
      fclose(fp);

      printf("content: %s\n", buffer);
    } else {
      printf("Error: Read open failed\n");
    }
  } else {
    printf("Initialization failed: %i\n", init_result);
  }

  printf("Done!\n");
  while (true) {
    sleep_ms(1);
  }
}




/*
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
*/
