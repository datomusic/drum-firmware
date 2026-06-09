#ifndef DRUM_SAMPLE_SLOT_MANAGER_H_
#define DRUM_SAMPLE_SLOT_MANAGER_H_

#include "etl/array.h"
#include "etl/optional.h"
#include "etl/string.h"
#include "etl/string_view.h"

#include "musin/hal/logger.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace drum {

/**
 * @brief Holds the audio data for all voices in RAM.
 *
 * Each voice owns a fixed buffer holding its current sample. A fifth
 * staging buffer is filled from the filesystem in small chunks on the
 * main loop, so loading never blocks audio rendering. A completed load
 * is committed to the voice's own buffer with a RAM copy, either when
 * the voice is triggered or when it falls idle.
 *
 * The audio path only ever reads the per-voice buffers; the filesystem
 * is only touched by update(), on the main loop.
 */
class SampleSlotManager {
public:
  static constexpr size_t NUM_VOICE_SLOTS = 4;
  /** Maximum sample length: 700 ms of 44.1 kHz 16-bit mono. */
  static constexpr size_t MAX_SLOT_SAMPLES = 30870;
  /** Samples read from the filesystem per update() call. */
  static constexpr size_t LOAD_CHUNK_SAMPLES = 2048;
  static constexpr size_t MAX_PATH_LENGTH = 64;

  explicit SampleSlotManager(musin::Logger &logger);
  ~SampleSlotManager();

  SampleSlotManager(const SampleSlotManager &) = delete;
  SampleSlotManager &operator=(const SampleSlotManager &) = delete;

  /**
   * @brief Starts loading a sample file into the staging buffer.
   *
   * A request for a different sample replaces any load in progress; a
   * request matching the in-flight or staged sample is a no-op.
   * @param voice_index Destination voice (0 to NUM_VOICE_SLOTS - 1).
   * @param sample_index Global sample slot index, used as identity.
   * @param path Filesystem path of the raw 16-bit PCM file.
   * @return true if the request was accepted or already satisfied.
   */
  bool request_load(uint8_t voice_index, size_t sample_index,
                    const etl::string_view &path);

  /**
   * @brief Pumps the in-progress load. Call frequently from the main loop.
   * Reads up to LOAD_CHUNK_SAMPLES from the open file per call.
   */
  void update();

  /**
   * @brief Loads a sample synchronously. Intended for boot-time preload.
   */
  bool load_blocking(uint8_t voice_index, size_t sample_index,
                     const etl::string_view &path);

  /** @brief True if the voice's own buffer holds the given sample. */
  bool voice_has_sample(uint8_t voice_index, size_t sample_index) const;

  /** @brief True if the staging buffer holds a completed load for this
   * voice and sample. */
  bool staging_ready_for(uint8_t voice_index, size_t sample_index) const;

  /** @brief True if a completed load is staged for this voice (any sample). */
  bool staging_ready_for_voice(uint8_t voice_index) const;

  /**
   * @brief Copies the staged sample into the destination voice's buffer.
   * Only valid when staging_ready_for_voice() is true. The staging buffer
   * becomes free for the next load.
   */
  void commit_staging();

  const int16_t *voice_data(uint8_t voice_index) const;
  uint32_t voice_length(uint8_t voice_index) const;
  etl::optional<size_t> voice_sample_index(uint8_t voice_index) const;

private:
  using SlotBuffer = etl::array<int16_t, MAX_SLOT_SAMPLES>;

  struct VoiceSlot {
    SlotBuffer buffer;
    uint32_t length = 0;
    etl::optional<size_t> sample_index;
  };

  enum class StagingState {
    Idle,
    Loading,
    Ready
  };

  void abort_load();

  musin::Logger &logger_;
  etl::array<VoiceSlot, NUM_VOICE_SLOTS> voice_slots_;

  SlotBuffer staging_buffer_;
  StagingState staging_state_ = StagingState::Idle;
  uint8_t staging_voice_ = 0;
  size_t staging_sample_index_ = 0;
  uint32_t staging_length_ = 0;
  etl::string<MAX_PATH_LENGTH> staging_path_;
  FILE *load_file_ = nullptr;
};

} // namespace drum

#endif // DRUM_SAMPLE_SLOT_MANAGER_H_
