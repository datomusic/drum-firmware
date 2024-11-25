#include <stdio.h>
#include "pico/stdlib.h"

int main(int argc, char *argv[]) {
  stdio_init_all();
  while(true){
    printf("Yeooo\n");
    sleep_ms(1000);
  }
  return 0;
}
