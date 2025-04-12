#ifndef SINE_SOURCE_H_FE102UHK
#define SINE_SOURCE_H_FE102UHK

#include "pico/audio.h"
#include <math.h>
#include <stdint.h>

#define SINE_WAVE_TABLE_LEN 2048
static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];

static void fill_sine_table() {
  for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
    sine_wave_table[i] = 32767 * cosf(i * 2 * (float)(M_PI / SINE_WAVE_TABLE_LEN));
  }
}

const uint32_t step0 = 0x060000;
const uint32_t step1 = 0x040000;
uint32_t pos0 = 0;
uint32_t pos1 = 0;
const uint32_t pos_max = 0x10000 * SINE_WAVE_TABLE_LEN;

static void sine_fill_buffer(const unsigned int volume, audio_buffer_t *buffer) {
  int32_t *samples = (int32_t *)buffer->buffer->bytes;
  for (unsigned int i = 0; i < buffer->max_sample_count; i++) {
    int32_t value0 = (volume * sine_wave_table[pos0 >> 16u]) << 8u;
    int32_t value1 = (volume * sine_wave_table[pos1 >> 16u]) << 8u;
    // use 32bit full scale
    samples[i * 2 + 0] = value0 + (value0 >> 16u); // L
    samples[i * 2 + 1] = value1 + (value1 >> 16u); // R
    pos0 += step0;
    pos1 += step1;
    if (pos0 >= pos_max)
      pos0 -= pos_max;
    if (pos1 >= pos_max)
      pos1 -= pos_max;
  }
  buffer->sample_count = buffer->max_sample_count;
}

#endif /* end of include guard: SINE_SOURCE_H_FE102UHK */
