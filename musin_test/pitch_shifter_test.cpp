#include <catch2/catch_test_macros.hpp>

#include "musin/audio/pitch_shifter.h"

#define CONST_BODY(BODY)                                                                           \
  constexpr auto body = []() {                                                                     \
    BODY;                                                                                          \
    return 0;                                                                                      \
  };                                                                                               \
  constexpr const auto _ = body();                                                                 \
  body();

template <int MAX_SAMPLES, int CHUNK_SIZE> struct DummyBufferReader : SampleReader {
  constexpr DummyBufferReader() {
    reset();
  }

  constexpr bool has_data() override {
    return active;
  }

  constexpr uint32_t read_samples(AudioBlock &block) override {
    uint32_t consumed = 0;
    uint32_t samples_written = 0;
    int16_t *out_iterator = block.begin();

    if (active) {
      for (int i = 0; i <= AUDIO_BLOCK_SAMPLES - CHUNK_SIZE; i += CHUNK_SIZE) {
        if (read_counter + CHUNK_SIZE > MAX_SAMPLES) {
          active = false;
          break;
        }

        for (int chunk = 0; chunk < CHUNK_SIZE; ++chunk) {
          *out_iterator = read_counter + 1;
          out_iterator++;
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

  constexpr void reset() override {
    read_counter = 0;
    active = true;
    remaining_length = MAX_SAMPLES;
  }

  int read_counter = 0;
  bool active = true;
  int remaining_length = MAX_SAMPLES;
};

TEST_CASE("PitchShifter reads samples") {
  CONST_BODY(({
    auto reader = DummyBufferReader<100, 4>();
    auto shifter = PitchShifter(reader);
    shifter.reset();

    shifter.set_speed(1);

    size_t total_samples_read = 0;
    size_t loop_counter = 0;
    int16_t buffer[100];

    int16_t *write_position = buffer;

    assert(AUDIO_BLOCK_SAMPLES == 20);

    while (shifter.has_data()) {
      AudioBlock block;
      auto samples_read = shifter.read_samples(block);
      assert(samples_read == AUDIO_BLOCK_SAMPLES);
      total_samples_read += samples_read;
      loop_counter += 1;
      for (size_t i = 0; i < samples_read; ++i) {
        *write_position = block[i];
        write_position++;
      }
    }

    for (size_t i = 0; i < 100; ++i) {
      assert(buffer[i] == i + 1);
    }

    assert(reader.read_counter == 100);
    assert(total_samples_read == 100);
    assert(loop_counter == 5);
  }));
}

TEST_CASE("PitchShifter fills buffer when speed is less than 1 and requested "
          "sample count is equal to chunk size of the underlying reader") {

  CONST_BODY(({
    const int CHUNK_SIZE = 4;
    auto reader = DummyBufferReader<4, CHUNK_SIZE>();
    PitchShifter shifter = PitchShifter(reader);
    shifter.reset();

    shifter.set_speed(0.5);

    AudioBlock block;
    auto samples_read = shifter.read_samples(block);
    assert(reader.read_counter == 4);
    assert(samples_read == AUDIO_BLOCK_SAMPLES);

    // Interpolated values
    assert(block[0] == 0);
    assert(block[1] == 0);
    assert(block[2] == 0);
    assert(block[3] == 0);
    assert(block[4] == 0);
    assert(block[5] == 0);
    assert(block[6] == 1);
    assert(block[7] == 1);
    assert(block[8] == 1);
    assert(block[9] == 2);
    assert(block[10] == 2);
    assert(block[11] == 3);
    assert(block[12] == 3);
    assert(block[13] == 2);
    assert(block[14] == 0);
    assert(block[15] == 0);
    assert(block[16] == 0);
    assert(block[17] == 0);
    assert(block[18] == 0);
    assert(block[19] == 0);
    assert(block[19] == 0);
    assert(block[19] == 0);
  }));
}

// TODO: Test that PitchShifter does not fill pad buffer with zeroes, if
// attempting to read a sample count which is not a multiple of the underlying
// reader chunk size. This should fail, and be fixed by introducing ChunkReader.
