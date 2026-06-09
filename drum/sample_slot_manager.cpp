#include "sample_slot_manager.h"

#include <algorithm>

namespace drum {

SampleSlotManager::SampleSlotManager(musin::Logger &logger) : logger_(logger) {
}

SampleSlotManager::~SampleSlotManager() {
  abort_load();
}

void SampleSlotManager::abort_load() {
  if (load_file_ != nullptr) {
    fclose(load_file_);
    load_file_ = nullptr;
  }
  staging_state_ = StagingState::Idle;
  staging_length_ = 0;
}

bool SampleSlotManager::request_load(uint8_t voice_index, size_t sample_index,
                                     const etl::string_view &path) {
  if (voice_index >= NUM_VOICE_SLOTS || path.size() > MAX_PATH_LENGTH) {
    return false;
  }
  if (voice_has_sample(voice_index, sample_index)) {
    return true;
  }
  const bool same_as_current = (staging_voice_ == voice_index) &&
                               (staging_sample_index_ == sample_index);
  if (staging_state_ != StagingState::Idle && same_as_current) {
    return true;
  }

  abort_load();

  staging_path_.assign(path.begin(), path.end());
  load_file_ = fopen(staging_path_.c_str(), "rb");
  if (load_file_ == nullptr) {
    logger_.error("Failed to open sample file:");
    logger_.error(staging_path_.c_str());
    return false;
  }

  staging_voice_ = voice_index;
  staging_sample_index_ = sample_index;
  staging_length_ = 0;
  staging_state_ = StagingState::Loading;
  return true;
}

void SampleSlotManager::update() {
  if (staging_state_ != StagingState::Loading) {
    return;
  }

  const size_t remaining = MAX_SLOT_SAMPLES - staging_length_;
  const size_t to_read = std::min(remaining, LOAD_CHUNK_SAMPLES);
  const size_t samples_read = fread(staging_buffer_.data() + staging_length_,
                                    sizeof(int16_t), to_read, load_file_);
  staging_length_ += samples_read;

  const bool file_exhausted = (samples_read < to_read);
  const bool buffer_full = (staging_length_ >= MAX_SLOT_SAMPLES);
  if (file_exhausted || buffer_full) {
    fclose(load_file_);
    load_file_ = nullptr;
    staging_state_ = StagingState::Ready;
  }
}

bool SampleSlotManager::load_blocking(uint8_t voice_index, size_t sample_index,
                                      const etl::string_view &path) {
  if (!request_load(voice_index, sample_index, path)) {
    return false;
  }
  if (voice_has_sample(voice_index, sample_index)) {
    return true;
  }
  while (staging_state_ == StagingState::Loading) {
    update();
  }
  if (staging_ready_for(voice_index, sample_index)) {
    commit_staging();
    return true;
  }
  return false;
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
  return staging_state_ == StagingState::Ready &&
         staging_voice_ == voice_index && staging_sample_index_ == sample_index;
}

bool SampleSlotManager::staging_ready_for_voice(uint8_t voice_index) const {
  return staging_state_ == StagingState::Ready && staging_voice_ == voice_index;
}

void SampleSlotManager::commit_staging() {
  if (staging_state_ != StagingState::Ready) {
    return;
  }
  VoiceSlot &slot = voice_slots_[staging_voice_];
  std::copy(staging_buffer_.begin(), staging_buffer_.begin() + staging_length_,
            slot.buffer.begin());
  slot.length = staging_length_;
  slot.sample_index = staging_sample_index_;
  staging_state_ = StagingState::Idle;
  staging_length_ = 0;
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
