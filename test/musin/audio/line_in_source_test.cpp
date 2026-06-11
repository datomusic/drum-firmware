#include "test_support.h"

#include "musin/audio/block.h"
#include "musin/audio/line_in_source.h"

#include <cstddef>

namespace {

struct FakeBlockReader : musin::audio::BlockReader {
  size_t samples_to_provide = 0;
  int16_t sample_value = 0;

  size_t read_samples(AudioBlock &out) override {
    const size_t count =
        samples_to_provide < out.size() ? samples_to_provide : out.size();
    for (size_t i = 0; i < count; ++i) {
      out[i] = sample_value;
    }
    return count;
  }
};

} // namespace

TEST_CASE("LineInSource passes samples through") {
  FakeBlockReader reader;
  reader.samples_to_provide = AUDIO_BLOCK_SAMPLES;
  reader.sample_value = 1234;

  musin::audio::LineInSource source(reader);

  AudioBlock block;
  source.fill_buffer(block);

  for (size_t i = 0; i < block.size(); ++i) {
    REQUIRE(block[i] == 1234);
  }
}

TEST_CASE("LineInSource zero-fills when the reader underruns") {
  FakeBlockReader reader;
  reader.samples_to_provide = 5;
  reader.sample_value = -321;

  musin::audio::LineInSource source(reader);

  AudioBlock block;
  source.fill_buffer(block);

  for (size_t i = 0; i < 5; ++i) {
    REQUIRE(block[i] == -321);
  }
  for (size_t i = 5; i < block.size(); ++i) {
    REQUIRE(block[i] == 0);
  }
}

TEST_CASE("LineInSource outputs silence when no data is available") {
  FakeBlockReader reader;

  musin::audio::LineInSource source(reader);

  AudioBlock block;
  block[0] = 42; // Stale data that must be overwritten.
  source.fill_buffer(block);

  for (size_t i = 0; i < block.size(); ++i) {
    REQUIRE(block[i] == 0);
  }
}
