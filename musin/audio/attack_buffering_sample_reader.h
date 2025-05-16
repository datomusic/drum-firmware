#ifndef MUSIN_AUDIO_ATTACK_BUFFERING_SAMPLE_READER_H_
#define MUSIN_AUDIO_ATTACK_BUFFERING_SAMPLE_READER_H_

#include "musin/audio/sample_reader.h" // Includes block.h for AudioBlock and AUDIO_BLOCK_SAMPLES
#include "drum/sb25_samples.h"      // For drum::SampleData
#include <algorithm> // For std::copy, std::min
#include <cstdint>

namespace musin {

class AttackBufferingSampleReader : public SampleReader {
public:
  constexpr AttackBufferingSampleReader()
      : sample_data_ptr_(nullptr),
        ram_read_pos_(0),
        flash_read_pos_(0),
        is_initialized_(false) {}

  constexpr explicit AttackBufferingSampleReader(const drum::SampleData& sample_data_ref)
      : sample_data_ptr_(&sample_data_ref),
        ram_read_pos_(0),
        flash_read_pos_(0),
        is_initialized_(true) {}

  constexpr void set_source(const drum::SampleData& sample_data_ref) {
    sample_data_ptr_ = &sample_data_ref;
    reset(); // Reset read positions for the new source
    is_initialized_ = true;
  }

  void reset() override {
    ram_read_pos_ = 0;
    flash_read_pos_ = 0;
  }

  bool has_data() override {
    if (!is_initialized_ || !sample_data_ptr_) {
      return false;
    }
    // Check if there's data left in RAM attack buffer or in flash
    return (ram_read_pos_ < sample_data_ptr_->get_attack_buffer_length()) ||
           (flash_read_pos_ < sample_data_ptr_->get_flash_data_length());
  }

  uint32_t read_samples(AudioBlock &out) override {
    if (!is_initialized_ || !sample_data_ptr_) { // Added null check for sample_data_ptr_
      std::fill(out.begin(), out.end(), 0);
      return 0;
    }

    uint32_t samples_written_total = 0;
    int16_t* out_ptr = out.begin();

    // 1. Read from RAM attack buffer if available
    if (ram_read_pos_ < sample_data_ptr_->get_attack_buffer_length()) {
      const uint32_t ram_samples_to_read = std::min(
          static_cast<uint32_t>(AUDIO_BLOCK_SAMPLES - samples_written_total),
          sample_data_ptr_->get_attack_buffer_length() - ram_read_pos_
      );

      if (ram_samples_to_read > 0) {
        const int16_t* ram_source_ptr = sample_data_ptr_->get_attack_buffer_ptr() + ram_read_pos_;
        std::copy(ram_source_ptr, ram_source_ptr + ram_samples_to_read, out_ptr);
        ram_read_pos_ += ram_samples_to_read;
        samples_written_total += ram_samples_to_read;
        out_ptr += ram_samples_to_read;
      }
    }

    // 2. Read from flash if more samples are needed and RAM buffer is exhausted
    if (samples_written_total < AUDIO_BLOCK_SAMPLES && flash_read_pos_ < sample_data_ptr_->get_flash_data_length()) {
      const uint32_t flash_samples_to_read = std::min(
          static_cast<uint32_t>(AUDIO_BLOCK_SAMPLES - samples_written_total),
          sample_data_ptr_->get_flash_data_length() - flash_read_pos_
      );

      if (flash_samples_to_read > 0) {
        const int16_t* flash_source_ptr = sample_data_ptr_->get_flash_data_ptr() + flash_read_pos_;
        std::copy(flash_source_ptr, flash_source_ptr + flash_samples_to_read, out_ptr);
        flash_read_pos_ += flash_samples_to_read;
        samples_written_total += flash_samples_to_read;
        // out_ptr += flash_samples_to_read; // Not needed as it's the last write stage
      }
    }

    // 3. Fill remaining part of the block with zeros if not enough samples were read
    if (samples_written_total < AUDIO_BLOCK_SAMPLES) {
      std::fill(out.begin() + samples_written_total, out.end(), 0);
    }

    return samples_written_total;
  }

private:
  const drum::SampleData* sample_data_ptr_; // Pointer to the sample data
  uint32_t ram_read_pos_;    // Current read position within the RAM attack buffer
  uint32_t flash_read_pos_;  // Current read position within the flash data portion
  bool is_initialized_;      // Flag to indicate if sample_data_ptr_ points to a valid SampleData
};

} // namespace musin

#endif // MUSIN_AUDIO_ATTACK_BUFFERING_SAMPLE_READER_H_
