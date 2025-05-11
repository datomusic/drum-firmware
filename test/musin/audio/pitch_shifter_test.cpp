#include "../test_support.h"
#include "musin/audio/pitch_shifter.h"

// Outputs a multiple of CHUNK_SIZE samples per call to read_samples, up to AUDIO_BLOCK_SAMPLES.
// If a full chunk cannot be returned, the last samples are skipped.
template <int SAMPLE_COUNT, int CHUNK_SIZE> struct DummyBufferReader : SampleReader {
  constexpr DummyBufferReader(etl::array<int16_t, SAMPLE_COUNT> samples) : samples(samples) {
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
        if (read_counter + CHUNK_SIZE > samples.size()) {
          active = false;
          break;
        }

        for (int chunk = 0; chunk < CHUNK_SIZE; ++chunk) {
          *out_iterator = samples[read_counter];
          out_iterator++;
          read_counter++;
        }

        consumed += CHUNK_SIZE;
        samples_written += CHUNK_SIZE;
      }
    } else {
      return 0;
    }

    if (read_counter == samples.size() || consumed == 0 || samples_written == 0) {
      active = false;
    }

    return samples_written;
  }

  constexpr void reset() override {
    read_counter = 0;
    active = true;
  }

  int read_counter = 0;
  bool active = true;
  etl::array<int16_t, SAMPLE_COUNT> samples;
};

TEST_CASE("PitchShifter reads samples") {
  CONST_BODY(({
    etl::array<int16_t, 100> samples;
    for (int i = 0; i < 100; ++i) {
      samples[i] = i + 1;
    }

    auto reader = DummyBufferReader<100, 4>(samples);
    auto shifter = PitchShifter(reader);
    shifter.reset();

    shifter.set_speed(1);

    size_t total_samples_read = 0;
    size_t loop_counter = 0;
    int16_t buffer[100];

    int16_t *write_position = buffer;

    REQUIRE(AUDIO_BLOCK_SAMPLES == 20);

    while (shifter.has_data()) {
      AudioBlock block;
      auto samples_read = shifter.read_samples(block);
      REQUIRE(samples_read == AUDIO_BLOCK_SAMPLES);
      total_samples_read += samples_read;
      loop_counter += 1;
      for (size_t i = 0; i < samples_read; ++i) {
        *write_position = block[i];
        write_position++;
      }
    }

    for (size_t i = 0; i < 100; ++i) {
      REQUIRE(buffer[i] == i + 1);
    }

    REQUIRE(reader.read_counter == 100);
    REQUIRE(total_samples_read == 100);
    REQUIRE(loop_counter == 5);
  }));
}

TEST_CASE("PitchShifter fills buffer when speed is less than 1 and requested "
          "sample count is equal to chunk size of the underlying reader") {

  CONST_BODY(({
    const int CHUNK_SIZE = 4;
    auto reader = DummyBufferReader<4, CHUNK_SIZE>({1, 2, 3, 4});
    PitchShifter shifter = PitchShifter(reader);
    shifter.reset();

    shifter.set_speed(0.5);

    AudioBlock block;
    auto samples_read = shifter.read_samples(block);
    REQUIRE(reader.read_counter == 4);
    REQUIRE(samples_read == AUDIO_BLOCK_SAMPLES);

    // Quadratic interpolated values
    /*
    REQUIRE(block[0] == 0);
    REQUIRE(block[1] == 0);
    REQUIRE(block[2] == 0);
    REQUIRE(block[3] == 0);
    REQUIRE(block[4] == 0);
    REQUIRE(block[5] == 0);
    REQUIRE(block[6] == 1);
    REQUIRE(block[7] == 1);
    REQUIRE(block[8] == 1);
    REQUIRE(block[9] == 2);
    REQUIRE(block[10] == 2);
    REQUIRE(block[11] == 3);
    REQUIRE(block[12] == 3);
    REQUIRE(block[13] == 2);
    REQUIRE(block[14] == 0);
    REQUIRE(block[15] == 0);
    REQUIRE(block[16] == 0);
    REQUIRE(block[17] == 0);
    REQUIRE(block[18] == 0);
    REQUIRE(block[19] == 0);
    REQUIRE(block[19] == 0);
    REQUIRE(block[19] == 0);
    */

    // Linear interpolated values
    // TODO: Replace with above implementation when PitchShifter uses quad interpolation again.
    REQUIRE(block[0] == 0);
    REQUIRE(block[1] == 0);
    REQUIRE(block[2] == 0);
    REQUIRE(block[3] == 0);
    REQUIRE(block[4] == 0);
    REQUIRE(block[5] == 0);
    REQUIRE(block[6] == 1);
    REQUIRE(block[7] == 1);
    REQUIRE(block[8] == 2);
    REQUIRE(block[9] == 2);
    REQUIRE(block[10] == 3);
    REQUIRE(block[11] == 3);
    REQUIRE(block[12] == 4);
    REQUIRE(block[13] == 2);
    REQUIRE(block[14] == 0);
    REQUIRE(block[15] == 0);
    REQUIRE(block[16] == 0);
    REQUIRE(block[17] == 0);
    REQUIRE(block[18] == 0);
    REQUIRE(block[19] == 0);
    REQUIRE(block[19] == 0);
    REQUIRE(block[19] == 0);
  }));
}

// TODO: Test that PitchShifter does not fill pad buffer with zeroes, if
// attempting to read a sample count which is not a multiple of the underlying
// reader chunk size. This should fail, and be fixed by introducing ChunkReader.
