#include "drum/sample_slot_manager.h"
#include "musin/hal/null_logger.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

musin::NullLogger logger;

// Writes a raw 16-bit PCM file where each sample equals base + offset,
// so loads can be identified by content.
std::string write_pcm_file(const char *name, size_t num_samples, int16_t base) {
  std::string path = std::string("./") + name;
  FILE *f = fopen(path.c_str(), "wb");
  REQUIRE(f != nullptr);
  std::vector<int16_t> samples(num_samples);
  for (size_t i = 0; i < num_samples; ++i) {
    samples[i] = static_cast<int16_t>(base + static_cast<int16_t>(i % 100));
  }
  fwrite(samples.data(), sizeof(int16_t), samples.size(), f);
  fclose(f);
  return path;
}

void pump_until_idle_or_ready(drum::SampleSlotManager &manager) {
  for (int i = 0; i < 1000; ++i) {
    manager.update();
  }
}

} // namespace

TEST_CASE("Chunked load completes and commits to the requested voice") {
  drum::SampleSlotManager manager(logger);
  const size_t length = 5000;
  auto path = write_pcm_file("slot_test_a.pcm", length, 1000);

  REQUIRE(manager.request_load(2, 7, path.c_str()));
  REQUIRE_FALSE(manager.staging_ready_for(2, 7));

  pump_until_idle_or_ready(manager);
  REQUIRE(manager.staging_ready_for(2, 7));
  REQUIRE(manager.staging_ready_for_voice(2));
  REQUIRE_FALSE(manager.voice_has_sample(2, 7));

  manager.commit_staging();
  REQUIRE(manager.voice_has_sample(2, 7));
  REQUIRE(manager.voice_length(2) == length);
  REQUIRE(manager.voice_sample_index(2).value() == 7);
  REQUIRE(manager.voice_data(2)[0] == 1000);
  REQUIRE(manager.voice_data(2)[99] == 1099);

  // Other voices untouched
  REQUIRE(manager.voice_length(0) == 0);
  REQUIRE_FALSE(manager.voice_sample_index(0).has_value());
  remove(path.c_str());
}

TEST_CASE("Oversized file is truncated at MAX_SLOT_SAMPLES") {
  drum::SampleSlotManager manager(logger);
  const size_t oversized = drum::SampleSlotManager::MAX_SLOT_SAMPLES + 4000;
  auto path = write_pcm_file("slot_test_big.pcm", oversized, 0);

  REQUIRE(manager.request_load(0, 1, path.c_str()));
  pump_until_idle_or_ready(manager);
  REQUIRE(manager.staging_ready_for(0, 1));
  manager.commit_staging();
  REQUIRE(manager.voice_length(0) == drum::SampleSlotManager::MAX_SLOT_SAMPLES);
  remove(path.c_str());
}

TEST_CASE("New request for a different sample replaces in-flight load") {
  drum::SampleSlotManager manager(logger);
  auto path_a = write_pcm_file("slot_test_c.pcm", 4000, 100);
  auto path_b = write_pcm_file("slot_test_d.pcm", 4000, 200);

  REQUIRE(manager.request_load(1, 10, path_a.c_str()));
  manager.update(); // partially loaded
  REQUIRE(manager.request_load(1, 11, path_b.c_str()));
  pump_until_idle_or_ready(manager);

  REQUIRE_FALSE(manager.staging_ready_for(1, 10));
  REQUIRE(manager.staging_ready_for(1, 11));
  manager.commit_staging();
  REQUIRE(manager.voice_data(1)[0] == 200);
  remove(path_a.c_str());
  remove(path_b.c_str());
}

TEST_CASE("Repeated request for the in-flight sample does not restart it") {
  drum::SampleSlotManager manager(logger);
  auto path = write_pcm_file("slot_test_e.pcm", 4000, 100);

  REQUIRE(manager.request_load(1, 10, path.c_str()));
  manager.update();
  REQUIRE(manager.request_load(1, 10, path.c_str()));
  manager.update();
  pump_until_idle_or_ready(manager);
  REQUIRE(manager.staging_ready_for(1, 10));
  remove(path.c_str());
}

TEST_CASE("Request for a sample the voice already holds is a no-op") {
  drum::SampleSlotManager manager(logger);
  auto path = write_pcm_file("slot_test_f.pcm", 1000, 100);

  REQUIRE(manager.load_blocking(3, 5, path.c_str()));
  REQUIRE(manager.voice_has_sample(3, 5));

  REQUIRE(manager.request_load(3, 5, path.c_str()));
  REQUIRE_FALSE(manager.staging_ready_for_voice(3));
  remove(path.c_str());
}

TEST_CASE("Missing file is reported as failure") {
  drum::SampleSlotManager manager(logger);
  REQUIRE_FALSE(manager.request_load(0, 3, "./does_not_exist.pcm"));
  REQUIRE_FALSE(manager.load_blocking(0, 3, "./does_not_exist.pcm"));
}
