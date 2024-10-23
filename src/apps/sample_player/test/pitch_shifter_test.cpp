#include <catch2/catch_test_macros.hpp>

#include "AudioSampleKick.h"
#include "audio_memory_reader.h"
#include "pitch_shifter.h"

struct DummyBufferReader : SampleReader {
  static const auto MAX_SAMPLES = 100;
  static const auto CHUNK_SIZE = 4;

  DummyBufferReader() {
    reset();
  }

  bool has_data() {
    return active;
  }

  uint32_t read_samples(int16_t *out, const uint16_t max_sample_count) {
    uint32_t consumed = 0, samples_written = 0;

    if (active) {
      for (int i = 0; i <= max_sample_count - CHUNK_SIZE; i += CHUNK_SIZE) {
        if (read_counter + CHUNK_SIZE > MAX_SAMPLES) {
          active = false;
          break;
        }

        for (int chunk = 0; chunk < CHUNK_SIZE; ++chunk) {
          *out++ = read_counter++;
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
  auto reader = DummyBufferReader();
  auto shifter = PitchShifter(reader);

  int16_t buffer[20];
  shifter.set_speed(1);

  auto total_samples_read = 0;
  auto loop_counter = 0;

  while (shifter.has_data()) {
    auto samples_read = shifter.read_samples(buffer, 23);
    REQUIRE(samples_read == 20);
    total_samples_read += samples_read;
    loop_counter += 1;
  }

  REQUIRE(reader.read_counter == 100);
  REQUIRE(total_samples_read == 100);
  REQUIRE(loop_counter == 5);
}

// TODO: Test that PitchShifter does not fill pad buffer with zeroes, if
// attempting to read a sample count which is not a multiple of the underlying
// reader chunk size. This should fail, and be fixed by introducing ChunkReader.
