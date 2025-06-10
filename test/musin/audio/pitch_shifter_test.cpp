#include "musin/audio/pitch_shifter.h"
#include "test_support.h"

// Outputs a multiple of CHUNK_SIZE samples per call to read_samples, up to AUDIO_BLOCK_SAMPLES.
// If a full chunk cannot be returned, the last samples are skipped.
template <int SAMPLE_COUNT, int CHUNK_SIZE> struct DummyBufferReader : SampleReader {
  constexpr DummyBufferReader(etl::array<int16_t, SAMPLE_COUNT> samples) : samples(samples) {
    reset();
  }

  constexpr bool has_data() override {
    return active;
  }

  constexpr bool read_next(int16_t &out) override {
    if (!active || read_counter >= samples.size()) {
      active = false;
      return false;
    }
    out = samples[read_counter++];
    if (read_counter >= samples.size()) {
      active = false;
    }
    return true;
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
    auto shifter = PitchShifter<CubicInterpolator>(reader);
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

TEST_CASE("PitchShifter fills buffer when speed is less than 1 and requested sample count is equal "
          "to chunk size of the underlying reader") {

  CONST_BODY(({
    const int CHUNK_SIZE = 4;
    auto reader =
        DummyBufferReader<16, CHUNK_SIZE>({1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000,
                                           10000, 11000, 12000, 13000, 14000, 15000, 16000});
    PitchShifter<CubicInterpolator> shifter = PitchShifter<CubicInterpolator>(reader);
    shifter.reset();

    shifter.set_speed(0.5f);

    AudioBlock block;
    auto samples_read = shifter.read_samples(block);
    REQUIRE(reader.read_counter == 16); // Verify we consumed all input
    REQUIRE(samples_read == AUDIO_BLOCK_SAMPLES);

    // First samples don't show proper interpolated values
    // because we prefill the buffer with 4 samples
    // TODO: fix the implementation so that this doesn't happen
    REQUIRE(block[0] == 1000); // Initial clamped value
    REQUIRE(block[1] == 1000);
    REQUIRE(block[2] == 1000);
    REQUIRE(block[3] == 1000);
    REQUIRE(block[4] == 1000);
    REQUIRE(block[5] == 875);
    REQUIRE(block[6] == 1000);
    REQUIRE(block[7] == 1375); // First interpolated value
    REQUIRE(block[8] == 2000);
    REQUIRE(block[9] == 2500);
    REQUIRE(block[10] == 3000);
    REQUIRE(block[11] == 3500);
    REQUIRE(block[12] == 4000);
    REQUIRE(block[13] == 4500);
  }));
}

// TODO: Test that PitchShifter does not fill pad buffer with zeroes, if
// attempting to read a sample count which is not a multiple of the underlying
// reader chunk size. This should fail, and be fixed by introducing ChunkReader.
