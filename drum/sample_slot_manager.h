#ifndef DRUM_SAMPLE_SLOT_MANAGER_H_
#define DRUM_SAMPLE_SLOT_MANAGER_H_

#include "etl/array.h"
#include "etl/optional.h"
#include "etl/string_view.h"

#include "musin/hal/logger.h"

#include <cstddef>
#include <cstdint>

namespace drum {

/**
 * @brief Holds the audio data for all voices in RAM.
 *
 * Each voice owns a fixed buffer holding its current sample. A fifth
 * staging buffer is loaded from the filesystem in one go (a sample file
 * reads in a few milliseconds) and committed to the voice's own buffer
 * with a RAM copy, either when the voice is triggered or when it falls
 * idle.
 *
 * The audio path only ever reads the per-voice buffers; the filesystem
 * is only touched by request_load(), on the main loop.
 */
class SampleSlotManager {
public:
  static constexpr size_t NUM_VOICE_SLOTS = 4;
  /** Maximum sample length: 900 ms of 44.1 kHz 16-bit mono. */
  static constexpr size_t MAX_SLOT_SAMPLES = 39690;

  explicit SampleSlotManager(musin::Logger &logger);

  SampleSlotManager(const SampleSlotManager &) = delete;
  SampleSlotManager &operator=(const SampleSlotManager &) = delete;

  /**
   * @brief Loads a sample file into the staging buffer.
   *
   * Reads the whole file synchronously; oversized files are truncated at
   * MAX_SLOT_SAMPLES. A no-op if the voice already holds the sample or
   * the staging buffer already does.
   * @param voice_index Destination voice (0 to NUM_VOICE_SLOTS - 1).
   * @param sample_index Global sample slot index, used as identity.
   * @param path Filesystem path of the raw 16-bit PCM file.
   * @return true if the sample is now staged or already held.
   */
  bool request_load(uint8_t voice_index, size_t sample_index,
                    const etl::string_view &path);

  /** @brief True if the voice's own buffer holds the given sample. */
  bool voice_has_sample(uint8_t voice_index, size_t sample_index) const;

  /** @brief True if the staging buffer holds a load for this voice and
   * sample. */
  bool staging_ready_for(uint8_t voice_index, size_t sample_index) const;

  /** @brief True if a load is staged for this voice (any sample). */
  bool staging_ready_for_voice(uint8_t voice_index) const;

  /**
   * @brief Copies the staged sample into the destination voice's buffer.
   * Only valid when staging_ready_for_voice() is true. The staging buffer
   * becomes free for the next load.
   */
  void commit_staging();

  /**
   * @brief Forgets any RAM copy of the given sample slot.
   *
   * Voices keep playing their current buffer, but the next trigger for
   * this sample index re-reads the file from flash. Used after a SysEx
   * transfer rewrites a sample so stale RAM data is not reused.
   */
  void invalidate_sample(size_t sample_index);

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

  musin::Logger &logger_;
  etl::array<VoiceSlot, NUM_VOICE_SLOTS> voice_slots_;

  SlotBuffer staging_buffer_;
  bool staging_ready_ = false;
  uint8_t staging_voice_ = 0;
  size_t staging_sample_index_ = 0;
  uint32_t staging_length_ = 0;
};

} // namespace drum

#endif // DRUM_SAMPLE_SLOT_MANAGER_H_
