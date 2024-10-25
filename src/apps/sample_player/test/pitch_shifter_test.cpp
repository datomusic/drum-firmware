#include <catch2/catch_test_macros.hpp>

#include "AudioSampleKick.h"
#include "audio_memory_reader.h"
#include "pitch_shifter.h"

template <int MAX_SAMPLES, int CHUNK_SIZE>
struct DummyBufferReader : SampleReader {
  DummyBufferReader() {
    reset();
  }

  bool has_data() {
    return active;
  }

  uint32_t read_samples(int16_t *out) {
    uint32_t consumed = 0, samples_written = 0;

    if (active) {
      for (int i = 0; i <= AUDIO_BLOCK_SAMPLES - CHUNK_SIZE; i += CHUNK_SIZE) {
        if (read_counter + CHUNK_SIZE > MAX_SAMPLES) {
          active = false;
          break;
        }

        for (int chunk = 0; chunk < CHUNK_SIZE; ++chunk) {
          *out++ = read_counter + 1;
          read_counter++;
        }

        consumed += CHUNK_SIZE;
        samples_written += CHUNK_SIZE;
      }
    } else {
      return samples_written;
    }

    if (consumed == 0 || samples_written == 0) {
      active = 0;
    } else {
      if (remaining_length > consumed) {
        remaining_length -= consumed;
      } else {
        active = 0;
      }
    }

    return samples_written;
  }

  void reset() {
    read_counter = 0;
    active = true;
    remaining_length = MAX_SAMPLES;
  }

  int read_counter = 0;
  bool active = true;
  int remaining_length = MAX_SAMPLES;
};

TEST_CASE("PitchShifter reads samples") {
  auto reader = DummyBufferReader<100, 4>();
  auto shifter = PitchShifter(reader);
  shifter.reset();

  int16_t buffer[100];
  shifter.set_speed(1);

  auto total_samples_read = 0;
  auto loop_counter = 0;

  int16_t *write_position = buffer;

  REQUIRE(AUDIO_BLOCK_SAMPLES == 20);

  while (shifter.has_data()) {
    auto samples_read = shifter.read_samples(write_position);
    REQUIRE(samples_read == AUDIO_BLOCK_SAMPLES);
    total_samples_read += samples_read;
    loop_counter += 1;
    write_position += samples_read;
  }

  REQUIRE(reader.read_counter == 100);
  REQUIRE(total_samples_read == 100);
  REQUIRE(loop_counter == 5);

  for (int i = 0; i < 100; ++i) {
    REQUIRE(buffer[i] == i + 1);
  }
}

TEST_CASE("PitchShifter fills buffer when speed is less than 1 and requested "
          "sample count is equal to chunk size of the underlying reader") {
  const int CHUNK_SIZE = 4;
  auto reader = DummyBufferReader<4, CHUNK_SIZE>();
  auto shifter = PitchShifter(reader);
  shifter.reset();

  int16_t buffer[AUDIO_BLOCK_SAMPLES];
  shifter.set_speed(0.5);

  auto samples_read = shifter.read_samples(buffer);
  REQUIRE(reader.read_counter == 4);
  REQUIRE(samples_read == AUDIO_BLOCK_SAMPLES);

  // Interpolated values
  REQUIRE(buffer[0] == 0);
  REQUIRE(buffer[1] == 0);
  REQUIRE(buffer[2] == 0);
  REQUIRE(buffer[3] == 0);
  REQUIRE(buffer[4] == 0);
  REQUIRE(buffer[5] == 0);
  REQUIRE(buffer[6] == 1);
  REQUIRE(buffer[7] == 1);
  REQUIRE(buffer[8] == 1);
  REQUIRE(buffer[9] == 2);
  REQUIRE(buffer[10] == 2);
  REQUIRE(buffer[11] == 3);
  REQUIRE(buffer[12] == 3);
  REQUIRE(buffer[13] == 2);
  REQUIRE(buffer[14] == 0);
  REQUIRE(buffer[15] == 0);
  REQUIRE(buffer[16] == 0);
  REQUIRE(buffer[17] == 0);
  REQUIRE(buffer[18] == 0);
  REQUIRE(buffer[19] == 0);
}

// TODO: Test that PitchShifter does not fill pad buffer with zeroes, if
// attempting to read a sample count which is not a multiple of the underlying
// reader chunk size. This should fail, and be fixed by introducing ChunkReader.
