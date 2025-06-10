#ifndef MUSIN_AUDIO_ATTACK_BUFFERING_SAMPLE_READER_H_
#define MUSIN_AUDIO_ATTACK_BUFFERING_SAMPLE_READER_H_

#include "musin/audio/buffered_reader.h" // Added include
#include "musin/audio/memory_reader.h"   // Added include
#include "musin/audio/sample_data.h"
#include "musin/hal/pico_dma_copier.h"
#include "musin/audio/sample_reader.h"
#include "musin/hal/debug_utils.h" // For underrun counter
#include "port/section_macros.h"
#include <algorithm> // For std::copy, std::min, std::fill
#include <cstdint>

namespace musin {

template <size_t NumFlashBufferBlocks = musin::DEFAULT_AUDIO_BLOCKS_PER_BUFFER_SLOT>
class AttackBufferingSampleReader : public SampleReader {
public:
  constexpr AttackBufferingSampleReader()
      : sample_data_ptr_(nullptr), ram_read_pos_(0), is_initialized_(false),
        flash_data_memory_reader_(), // Default constructed
        flash_data_buffered_reader_(flash_data_memory_reader_) {
  }

  constexpr explicit AttackBufferingSampleReader(const SampleData &sample_data_ref)
      : sample_data_ptr_(&sample_data_ref), ram_read_pos_(0), is_initialized_(true),
        flash_data_memory_reader_(sample_data_ref.get_flash_data_ptr(),
                                  sample_data_ref.get_flash_data_length()),
        flash_data_buffered_reader_(flash_data_memory_reader_) {
  }

  constexpr void set_source(const SampleData &sample_data_ref) {
    sample_data_ptr_ = &sample_data_ref;
    flash_data_memory_reader_.set_source(sample_data_ptr_->get_flash_data_ptr(),
                                         sample_data_ptr_->get_flash_data_length());
    reset(); // Resets ram_read_pos_ and flash_data_buffered_reader_
    is_initialized_ = true;
  }

  constexpr void reset() override {
    ram_read_pos_ = 0;
    flash_data_buffered_reader_.reset();
  }

  constexpr bool __time_critical_func(has_data)() override {
    if (!is_initialized_ || !sample_data_ptr_) {
      return false;
    }
    // Check if there's data left in RAM attack buffer
    if (ram_read_pos_ < sample_data_ptr_->get_attack_buffer_length()) {
      return true;
    }
    // If RAM is exhausted, check the buffered reader for flash data
    return flash_data_buffered_reader_.has_data();
  }

  constexpr uint32_t __time_critical_func(read_samples)(AudioBlock &out) override {
    if (!is_initialized_ || !sample_data_ptr_) { // Added null check for sample_data_ptr_
      std::fill(out.begin(), out.end(), 0);
      return 0;
    }

    uint32_t samples_written_total = 0;
    int16_t *out_ptr = out.begin();

    // 1. Read from RAM attack buffer if available
    if (ram_read_pos_ < sample_data_ptr_->get_attack_buffer_length()) {
      const uint32_t ram_samples_to_read =
          std::min(static_cast<uint32_t>(AUDIO_BLOCK_SAMPLES - samples_written_total),
                   sample_data_ptr_->get_attack_buffer_length() - ram_read_pos_);

      if (ram_samples_to_read > 0) {
        const int16_t *ram_source_ptr = sample_data_ptr_->get_attack_buffer_ptr() + ram_read_pos_;
        std::copy(ram_source_ptr, ram_source_ptr + ram_samples_to_read, out_ptr);
        ram_read_pos_ += ram_samples_to_read;
        samples_written_total += ram_samples_to_read;
        out_ptr += ram_samples_to_read;
      }
    }

    // 2. Read from buffered flash data if more samples are needed and RAM buffer is exhausted
    if (samples_written_total < AUDIO_BLOCK_SAMPLES) {
      uint32_t samples_needed_from_flash = AUDIO_BLOCK_SAMPLES - samples_written_total;
      uint32_t samples_read_from_flash =
          flash_data_buffered_reader_.read_buffered_chunk(out_ptr, samples_needed_from_flash);

      samples_written_total += samples_read_from_flash;
      out_ptr += samples_read_from_flash; // Advance out_ptr by the number of samples read
    }

    // 3. Fill remaining part of the block with zeros if not enough samples were read
    if (samples_written_total < AUDIO_BLOCK_SAMPLES) {
      std::fill(out.begin() + samples_written_total, out.end(), 0);
    }

    if (samples_written_total < out.size()) {
      // Check if we should have been able to provide more data
      bool still_has_ram_data = ram_read_pos_ < sample_data_ptr_->get_attack_buffer_length();
      // flash_data_buffered_reader_.has_data() reflects state *after* reads in this block
      if (still_has_ram_data || flash_data_buffered_reader_.has_data()) {
        musin::hal::DebugUtils::g_attack_buffer_reader_underruns++;
      }
    }

    return samples_written_total;
  }

  constexpr bool __time_critical_func(read_next)(int16_t &out) override {
    if (!is_initialized_ || !sample_data_ptr_) {
      out = 0; // Provide a default value for safety, though caller should check return
      return false;
    }

    // 1. Try to read from RAM attack buffer
    if (ram_read_pos_ < sample_data_ptr_->get_attack_buffer_length()) {
      out = sample_data_ptr_->get_attack_buffer_ptr()[ram_read_pos_];
      ram_read_pos_++;
      return true;
    }

    // 2. If RAM is exhausted, try to read from buffered flash data
    return flash_data_buffered_reader_.read_next(out);
  }

private:
  const SampleData *sample_data_ptr_; // Pointer to the sample data
  uint32_t ram_read_pos_;             // Current read position within the RAM attack buffer
  bool is_initialized_; // Flag to indicate if sample_data_ptr_ points to a valid SampleData

  // Internal readers for handling the flash data portion with double buffering
  musin::MemorySampleReader flash_data_memory_reader_;
  musin::BufferedReader<NumFlashBufferBlocks, musin::hal::PicoDmaCopier>
      flash_data_buffered_reader_;
};

} // namespace musin

#endif // MUSIN_AUDIO_ATTACK_BUFFERING_SAMPLE_READER_H_
