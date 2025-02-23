#include <pico/stdlib.h>
#include <stdio.h>

extern "C" bool init_filesystem(bool force_format);
extern "C" int printf(const char*, ...);

int main(void) {
  stdio_init_all();
  printf("Startup\n");

  sleep_ms(1000);

  // Give host some time to catch up, otherwise messages can be lost.
  while (true) {
    sleep_ms(1);
  }
}
