
#include "etl/array.h"
#include "musin/audio/block.h" // For AUDIO_BLOCK_SAMPLES
#include <cstdint>
#include <algorithm> // For std::min

class SampleData {
public:
  SampleData(const int16_t* full_sample_data, uint32_t full_sample_length)
    : attack_buffer_ptr_(nullptr),
      attack_buffer_length_(0),
      flash_data_ptr_(nullptr),
      flash_data_length_(0) {

    if (full_sample_data == nullptr || full_sample_length == 0) {
      // Handle empty or null input, perhaps log or assert
      // For now, leave members as initialized (nullptr/0)
      return;
    }

    // Determine how many samples to copy to the RAM attack buffer
    attack_buffer_length_ = std::min(full_sample_length, static_cast<uint32_t>(AUDIO_BLOCK_SAMPLES));

    // Copy the attack portion to the RAM buffer
    std::copy(full_sample_data, full_sample_data + attack_buffer_length_, attack_buffer_ram_.begin());
    attack_buffer_ptr_ = attack_buffer_ram_.begin();

    // Set up pointers and lengths for the remaining data in flash
    if (full_sample_length > attack_buffer_length_) {
      flash_data_ptr_ = full_sample_data + attack_buffer_length_;
      flash_data_length_ = full_sample_length - attack_buffer_length_;
    } else {
      // Sample is shorter than or equal to one block, so no remaining flash data
      flash_data_ptr_ = nullptr;
      flash_data_length_ = 0;
    }
  }

  // Public accessors
  [[nodiscard]] const int16_t* get_attack_buffer_ptr() const { return attack_buffer_ptr_; }
  [[nodiscard]] uint32_t get_attack_buffer_length() const { return attack_buffer_length_; }
  [[nodiscard]] const int16_t* get_flash_data_ptr() const { return flash_data_ptr_; }
  [[nodiscard]] uint32_t get_flash_data_length() const { return flash_data_length_; }

private:
  etl::array<int16_t, AUDIO_BLOCK_SAMPLES> attack_buffer_ram_;
  const int16_t* attack_buffer_ptr_; // Points to the start of attack_buffer_ram_
  uint32_t attack_buffer_length_;    // Actual number of samples in attack_buffer_ram_

  const int16_t* flash_data_ptr_;    // Points to data in flash after the attack portion
  uint32_t flash_data_length_;       // Length of the data in flash after the attack portion
};

extern const etl::array<SampleData, 32> all_samples;
