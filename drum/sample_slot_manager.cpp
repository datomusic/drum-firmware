#include "sample_slot_manager.h"

#include "etl/string.h"

#include <algorithm>
#include <cstdio>

namespace drum {

namespace {
constexpr size_t MAX_PATH_LENGTH = 64;
}

SampleSlotManager::SampleSlotManager(musin::Logger &logger) : logger_(logger) {
}

bool SampleSlotManager::request_load(uint8_t voice_index, size_t sample_index,
                                     const etl::string_view &path) {
  if (voice_index >= NUM_VOICE_SLOTS || path.size() > MAX_PATH_LENGTH) {
    return false;
  }
  if (voice_has_sample(voice_index, sample_index) ||
      staging_ready_for(voice_index, sample_index)) {
    return true;
  }

  const etl::string<MAX_PATH_LENGTH> terminated_path(path.begin(), path.end());
  FILE *file = fopen(terminated_path.c_str(), "rb");
  if (file == nullptr) {
    logger_.error("Failed to open sample file:");
    logger_.error(terminated_path.c_str());
    return false;
  }

  staging_ready_ = false;
  staging_length_ = static_cast<uint32_t>(
      fread(staging_buffer_.data(), sizeof(int16_t), MAX_SLOT_SAMPLES, file));
  fclose(file);

  staging_voice_ = voice_index;
  staging_sample_index_ = sample_index;
  staging_ready_ = true;
  return true;
}

bool SampleSlotManager::voice_has_sample(uint8_t voice_index,
                                         size_t sample_index) const {
  if (voice_index >= NUM_VOICE_SLOTS) {
    return false;
  }
  const VoiceSlot &slot = voice_slots_[voice_index];
  return slot.sample_index.has_value() &&
         slot.sample_index.value() == sample_index;
}

bool SampleSlotManager::staging_ready_for(uint8_t voice_index,
                                          size_t sample_index) const {
  return staging_ready_ && staging_voice_ == voice_index &&
         staging_sample_index_ == sample_index;
}

bool SampleSlotManager::staging_ready_for_voice(uint8_t voice_index) const {
  return staging_ready_ && staging_voice_ == voice_index;
}

void SampleSlotManager::commit_staging() {
  if (!staging_ready_) {
    return;
  }
  VoiceSlot &slot = voice_slots_[staging_voice_];
  std::copy(staging_buffer_.begin(), staging_buffer_.begin() + staging_length_,
            slot.buffer.begin());
  slot.length = staging_length_;
  slot.sample_index = staging_sample_index_;
  staging_ready_ = false;
}

const int16_t *SampleSlotManager::voice_data(uint8_t voice_index) const {
  if (voice_index >= NUM_VOICE_SLOTS) {
    return nullptr;
  }
  return voice_slots_[voice_index].buffer.data();
}

uint32_t SampleSlotManager::voice_length(uint8_t voice_index) const {
  if (voice_index >= NUM_VOICE_SLOTS) {
    return 0;
  }
  return voice_slots_[voice_index].length;
}

etl::optional<size_t>
SampleSlotManager::voice_sample_index(uint8_t voice_index) const {
  if (voice_index >= NUM_VOICE_SLOTS) {
    return etl::nullopt;
  }
  return voice_slots_[voice_index].sample_index;
}

} // namespace drum
