#include <pico/stdlib.h>
#include <stdio.h>
#include "core/filesystem.h"
// #include "filesystem/vfs.h"

int main(void) {
  stdio_init_all();
  sleep_ms(2000);

  printf("Startup 4\n");

  // Give host some time to catch up, otherwise messages can be lost.

  printf("\n\n");
  printf("Initializing fs\n");
  sleep_ms(1000);
  const auto init_result = init_filesystem(true);
  if (init_result) {
    printf("fs initialized\n");
    printf("Opening file for writing\n");
    // Path must start with backslash, otherwise writing freezes (or crashes?).
    FILE *fp = fopen("/dat", "w");
    if (fp) {
      printf("fp: %i\n", (int)fp);

      printf("Writing...\n");
      const int ret = fprintf(fp, "Rhythm is a yeo!\n");
      printf("Res: %i\n", ret);

      printf("Closing file\n");
      fflush(fp);
      fclose(fp);
    }else{
      printf("Failed opening writable file\n");
    }
    fp = 0;

    sleep_ms(1000);

    printf("Opening for reading\n");
    fp = fopen("/dat", "r");

    printf("Reading\n");
    char buffer[64] = {0};
    fgets(buffer, sizeof(buffer), fp);

    printf("Closing file\n");
    fclose(fp);

    printf("content: %s\n", buffer);
  } else {
    printf("Initialization failed: %i\n", init_result);
  }

  printf("Done!\n");
  while (true) {
    printf("tick\n");
    sleep_ms(1000);
  }
}
