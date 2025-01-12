#include <math.h>
#include <stdio.h>
#include "pico/stdlib.h"

int main() {
  stdio_init_all();

  while (true) {
    int c = getchar_timeout_us(0);
  }

  puts("\n");
  return 0;
}
