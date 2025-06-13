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
    auto shifter = PitchShifter(reader);
    shifter.reset();

    shifter.set_speed(1);
    shifter.set_interpolator(&CubicInterpolator::interpolate);

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
    auto shifter = PitchShifter(reader);
    shifter.reset();

    shifter.set_speed(0.5f);
    shifter.set_interpolator(&CubicInterpolator::interpolate);

    AudioBlock block;
    auto samples_read = shifter.read_samples(block);
    // no longer relevant as the pitch shifter reads lazily
    // REQUIRE(reader.read_counter == 16); // Verify we consumed all input
    REQUIRE(samples_read == AUDIO_BLOCK_SAMPLES);

    // The initial samples are based on the Catmull-Rom interpolation of the
    // source data. The first sample is the same as the source's first sample
    // because the interpolation position has a fractional part of 0.
    REQUIRE(block[0] == 1000);
    REQUIRE(block[1] == 1437);
    REQUIRE(block[2] == 2000);
    REQUIRE(block[3] == 2500);
    REQUIRE(block[4] == 3000);
    REQUIRE(block[5] == 3500);
    REQUIRE(block[6] == 4000);
    REQUIRE(block[7] == 4500);
    REQUIRE(block[8] == 5000);
    REQUIRE(block[9] == 5500);
    REQUIRE(block[10] == 6000);
    REQUIRE(block[11] == 6500);
    REQUIRE(block[12] == 7000);
    REQUIRE(block[13] == 7500);
  }));
}

// TODO: Test that PitchShifter does not fill pad buffer with zeroes, if
// attempting to read a sample count which is not a multiple of the underlying
// reader chunk size. This should fail, and be fixed by introducing ChunkReader.

TEST_CASE("HardwareLinearInterpolator correctly configures and uses the hardware") {
  // This test verifies runtime hardware interaction (via mocks) and is not
  // constexpr-compatible.

  // This test verifies that the HardwareLinearInterpolator correctly interacts
  // with the mock hardware interpolator (interp0).

  // Reset mock hardware state before the test
  reset_mock_interp_state();

  // Call the interpolator. This should trigger initialize_hardware().
  const int16_t y1 = 1000;
  const int16_t y2 = 2000;
  const float mu = 0.5f;
  HardwareLinearInterpolator::interpolate(0, y1, y2, 0, mu);

  // 1. Verify that the hardware was initialized correctly
  REQUIRE(mock_interp0_lane0_cfg.blend == true);
  REQUIRE(mock_interp0_lane1_cfg.is_signed == true);

  // 2. Verify that the two sample values were loaded into BASE registers
  REQUIRE(static_cast<int16_t>(interp0->base[0]) == y1);
  REQUIRE(static_cast<int16_t>(interp0->base[1]) == y2);

  // 3. Verify that the fraction was loaded into the accumulator
  const uint32_t expected_fraction = static_cast<uint32_t>(mu * 255.0f);
  REQUIRE(interp0->accum[1] == expected_fraction);
  REQUIRE(interp0->accum[1] >= 0);
  REQUIRE(interp0->accum[1] <= 255);
}

TEST_CASE("PitchShifter with NearestNeighborInterpolator works correctly") {
  CONST_BODY(({
    const int CHUNK_SIZE = 4;
    auto reader =
        DummyBufferReader<16, CHUNK_SIZE>({1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000,
                                           10000, 11000, 12000, 13000, 14000, 15000, 16000});
    auto shifter = PitchShifter(reader);
    shifter.reset();

    shifter.set_speed(0.5f);
    shifter.set_interpolator(&NearestNeighborInterpolator::interpolate);

    AudioBlock block;
    auto samples_read = shifter.read_samples(block);
    REQUIRE(samples_read == AUDIO_BLOCK_SAMPLES);

    // With nearest neighbor, mu < 0.5 rounds down (y1), mu >= 0.5 rounds up (y2).
    REQUIRE(block[0] == 1000); // mu = 0.0
    REQUIRE(block[1] == 2000); // mu = 0.5
    REQUIRE(block[2] == 2000); // mu = 0.0
    REQUIRE(block[3] == 3000); // mu = 0.5
    REQUIRE(block[4] == 3000); // mu = 0.0
    REQUIRE(block[5] == 4000); // mu = 0.5
    REQUIRE(block[6] == 4000); // mu = 0.0
    REQUIRE(block[7] == 5000); // mu = 0.5
  }));
}

TEST_CASE("PitchShifter with QuadraticInterpolator works correctly") {
  CONST_BODY(({
    const int CHUNK_SIZE = 4;
    auto reader =
        DummyBufferReader<16, CHUNK_SIZE>({1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000,
                                           10000, 11000, 12000, 13000, 14000, 15000, 16000});
    auto shifter = PitchShifter(reader);
    shifter.reset();

    shifter.set_speed(0.5f);
    shifter.set_interpolator(&QuadraticInterpolator::interpolate);

    AudioBlock block;
    auto samples_read = shifter.read_samples(block);
    REQUIRE(samples_read == AUDIO_BLOCK_SAMPLES);

    // Check interpolated values
    REQUIRE(block[0] == 1000); // mu=0.0
    REQUIRE(block[1] == 1375); // mu=0.5, y0=1000, y1=1000, y2=2000
    REQUIRE(block[2] == 2000); // mu=0.0
    REQUIRE(block[3] == 2500); // mu=0.5, y0=1000, y1=2000, y2=3000 (linear)
    REQUIRE(block[4] == 3000); // mu=0.0
    REQUIRE(block[5] == 3500); // mu=0.5, y0=2000, y1=3000, y2=4000 (linear)
  }));
}

TEST_CASE("PitchShifter with QuadraticInterpolatorInt works correctly") {
  CONST_BODY(({
    const int CHUNK_SIZE = 4;
    auto reader =
        DummyBufferReader<16, CHUNK_SIZE>({1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000,
                                           10000, 11000, 12000, 13000, 14000, 15000, 16000});
    auto shifter = PitchShifter(reader);
    shifter.reset();

    shifter.set_speed(0.5f);
    shifter.set_interpolator(&QuadraticInterpolatorInt::interpolate);

    AudioBlock block;
    auto samples_read = shifter.read_samples(block);
    REQUIRE(samples_read == AUDIO_BLOCK_SAMPLES);

    // The integer version should produce identical results for this input
    REQUIRE(block[0] == 1000);
    REQUIRE(block[1] == 1375);
    REQUIRE(block[2] == 2000);
    REQUIRE(block[3] == 2500);
    REQUIRE(block[4] == 3000);
    REQUIRE(block[5] == 3500);
  }));
}

TEST_CASE("PitchShifter with CubicInterpolatorOptimized works correctly") {
  CONST_BODY(({
    const int CHUNK_SIZE = 4;
    auto reader =
        DummyBufferReader<16, CHUNK_SIZE>({1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000,
                                           10000, 11000, 12000, 13000, 14000, 15000, 16000});
    auto shifter = PitchShifter(reader);
    shifter.reset();

    shifter.set_speed(0.5f);
    shifter.set_interpolator(&CubicInterpolatorOptimized::interpolate);

    AudioBlock block;
    auto samples_read = shifter.read_samples(block);
    REQUIRE(samples_read == AUDIO_BLOCK_SAMPLES);

    // The optimized version should produce identical results to the original.
    REQUIRE(block[0] == 1000);
    REQUIRE(block[1] == 1437);
    REQUIRE(block[2] == 2000);
    REQUIRE(block[3] == 2500);
    REQUIRE(block[4] == 3000);
    REQUIRE(block[5] == 3500);
  }));
}

TEST_CASE("PitchShifter with CubicInterpolatorInt works correctly") {
  CONST_BODY(({
    const int CHUNK_SIZE = 4;
    auto reader =
        DummyBufferReader<16, CHUNK_SIZE>({1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000,
                                           10000, 11000, 12000, 13000, 14000, 15000, 16000});
    auto shifter = PitchShifter(reader);
    shifter.reset();

    shifter.set_speed(0.5f);
    shifter.set_interpolator(&CubicInterpolatorInt::interpolate);

    AudioBlock block;
    auto samples_read = shifter.read_samples(block);
    REQUIRE(samples_read == AUDIO_BLOCK_SAMPLES);

    // The integer version should produce nearly identical results.
    // For this input, the results are identical.
    REQUIRE(block[0] == 1000);
    REQUIRE(block[1] == 1437);
    REQUIRE(block[2] == 2000);
    REQUIRE(block[3] == 2500);
    REQUIRE(block[4] == 3000);
    REQUIRE(block[5] == 3500);
  }));
}
