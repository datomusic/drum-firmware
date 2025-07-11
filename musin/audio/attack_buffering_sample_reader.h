#ifndef MUSIN_AUDIO_ATTACK_BUFFERING_SAMPLE_READER_H_
#define MUSIN_AUDIO_ATTACK_BUFFERING_SAMPLE_READER_H_

#include "etl/string_view.h"
#include "musin/audio/buffered_reader.h"
#include "musin/audio/memory_reader.h"
#include "musin/audio/sample_data.h"
#include "musin/audio/sample_reader.h"
#include "musin/audio/unbuffered_file_sample_reader.h"
#include "musin/hal/debug_utils.h" // For underrun counter
#include "musin/hal/pico_dma_copier.h"
#include "port/section_macros.h"
#include <algorithm> // For std::copy, std::min, std::fill
#include <cstdint>

namespace musin {

template <size_t NumFlashBufferBlocks =
              musin::DEFAULT_AUDIO_BLOCKS_PER_BUFFER_SLOT>
class AttackBufferingSampleReader : public SampleReader {
private:
  // A private proxy class to abstract the sustain data source (memory vs file)
  class SustainReader : public SampleReader {
  public:
    void set_active_reader(SampleReader *reader) {
      active_reader_ = reader;
    }

    void reset() override {
      if (active_reader_)
        active_reader_->reset();
    }
    bool has_data() override {
      return active_reader_ ? active_reader_->has_data() : false;
    }
    uint32_t read_samples(AudioBlock &out) override {
      if (active_reader_) {
        return active_reader_->read_samples(out);
      }
      std::fill(out.begin(), out.end(), 0);
      return 0;
    }
    bool read_next(int16_t &out) override {
      if (active_reader_) {
        return active_reader_->read_next(out);
      }
      out = 0;
      return false;
    }

  private:
    SampleReader *active_reader_ = nullptr;
  };

public:
  AttackBufferingSampleReader()
      : ram_read_pos_(0), attack_buffer_length_(0),
        source_type_(SourceType::NONE),
        flash_data_buffered_reader_(sustain_reader_proxy_) {
    sustain_reader_proxy_.set_active_reader(&flash_data_memory_reader_);
  }

  // This constructor is for backward compatibility.
  explicit AttackBufferingSampleReader(const SampleData &sample_data_ref)
      : ram_read_pos_(0), attack_buffer_length_(0),
        source_type_(SourceType::NONE),
        flash_data_buffered_reader_(sustain_reader_proxy_) {
    set_source(sample_data_ref);
  }

  /**
   * @brief Configures the reader to use a memory-based sample.
   * This maintains the original behavior.
   * @param sample_data_ref A reference to the SampleData object.
   */
  void set_source(const SampleData &sample_data_ref) {
    source_type_ = SourceType::FROM_SAMPLEDATA;
    sample_data_ptr_ = &sample_data_ref;
    sustain_reader_proxy_.set_active_reader(&flash_data_memory_reader_);
    flash_data_memory_reader_.set_source(
        sample_data_ptr_->get_flash_data_ptr(),
        sample_data_ptr_->get_flash_data_length());
    reset(); // Resets ram_read_pos_ and flash_data_buffered_reader_
  }

  /**
   * @brief Configures the reader to stream from a file.
   * It pre-loads the first block of the file as the attack portion.
   * @param path The path to the sample file.
   * @return true if the file was opened and the attack was loaded, false
   * otherwise.
   */
  bool load(const etl::string_view &path) {
    flash_data_file_reader_.close();
    if (!flash_data_file_reader_.open(path)) {
      source_type_ = SourceType::NONE;
      return false;
    }

    source_type_ = SourceType::FROM_FILE;
    sustain_reader_proxy_.set_active_reader(&flash_data_file_reader_);
    reset(); // Rewinds file to beginning via proxy

    // Preload the attack from the start of the file.
    attack_buffer_length_ =
        flash_data_file_reader_.read_samples(attack_buffer_ram_);

    // File is now positioned after the attack, ready for sustain reading.
    return true;
  }

  void reset() override {
    ram_read_pos_ = 0;
    flash_data_buffered_reader_.reset();
  }

  bool __time_critical_func(has_data)() override {
    if (source_type_ == SourceType::NONE) {
      return false;
    }

    if (source_type_ == SourceType::FROM_SAMPLEDATA) {
      if (!sample_data_ptr_)
        return false;
      // Check if there's data left in RAM attack buffer from SampleData
      if (ram_read_pos_ < sample_data_ptr_->get_attack_buffer_length()) {
        return true;
      }
    } else { // FROM_FILE
      // Check if there's data left in our internal RAM attack buffer
      if (ram_read_pos_ < attack_buffer_length_) {
        return true;
      }
    }

    // If RAM is exhausted, check the buffered reader for sustain data
    return flash_data_buffered_reader_.has_data();
  }

  uint32_t __time_critical_func(read_samples)(AudioBlock &out) override {
    if (source_type_ == SourceType::NONE) {
      std::fill(out.begin(), out.end(), 0);
      return 0;
    }

    uint32_t samples_written_total = 0;
    int16_t *out_ptr = out.begin();

    // 1. Read from RAM attack buffer if available
    const int16_t *attack_source_ptr = nullptr;
    uint32_t attack_length = 0;

    if (source_type_ == SourceType::FROM_SAMPLEDATA && sample_data_ptr_) {
      attack_source_ptr = sample_data_ptr_->get_attack_buffer_ptr();
      attack_length = sample_data_ptr_->get_attack_buffer_length();
    } else if (source_type_ == SourceType::FROM_FILE) {
      attack_source_ptr = attack_buffer_ram_.begin();
      attack_length = attack_buffer_length_;
    }

    if (attack_source_ptr && ram_read_pos_ < attack_length) {
      const uint32_t ram_samples_to_read = std::min(
          static_cast<uint32_t>(AUDIO_BLOCK_SAMPLES - samples_written_total),
          attack_length - ram_read_pos_);

      if (ram_samples_to_read > 0) {
        std::copy(attack_source_ptr + ram_read_pos_,
                  attack_source_ptr + ram_read_pos_ + ram_samples_to_read,
                  out_ptr);
        ram_read_pos_ += ram_samples_to_read;
        samples_written_total += ram_samples_to_read;
        out_ptr += ram_samples_to_read;
      }
    }

    // 2. Read from buffered sustain data if more samples are needed and RAM
    // buffer is exhausted
    if (samples_written_total < AUDIO_BLOCK_SAMPLES) {
      uint32_t samples_needed_from_sustain =
          AUDIO_BLOCK_SAMPLES - samples_written_total;
      uint32_t samples_read_from_sustain =
          flash_data_buffered_reader_.read_buffered_chunk(
              out_ptr, samples_needed_from_sustain);

      samples_written_total += samples_read_from_sustain;
      out_ptr += samples_read_from_sustain; // Advance out_ptr by the number of
                                            // samples read
    }

    // 3. Fill remaining part of the block with zeros if not enough samples were
    // read
    if (samples_written_total < AUDIO_BLOCK_SAMPLES) {
      std::fill(out.begin() + samples_written_total, out.end(), 0);
    }

    if (samples_written_total < out.size()) {
      // Check if we should have been able to provide more data
      if (has_data()) {
        musin::hal::DebugUtils::g_attack_buffer_reader_underruns++;
      }
    }

    return samples_written_total;
  }

  bool __time_critical_func(read_next)(int16_t &out) override {
    if (source_type_ == SourceType::NONE) {
      out = 0;
      return false;
    }

    // 1. Try to read from RAM attack buffer
    if (source_type_ == SourceType::FROM_SAMPLEDATA && sample_data_ptr_) {
      if (ram_read_pos_ < sample_data_ptr_->get_attack_buffer_length()) {
        out = sample_data_ptr_->get_attack_buffer_ptr()[ram_read_pos_];
        ram_read_pos_++;
        return true;
      }
    } else if (source_type_ == SourceType::FROM_FILE) {
      if (ram_read_pos_ < attack_buffer_length_) {
        out = attack_buffer_ram_[ram_read_pos_];
        ram_read_pos_++;
        return true;
      }
    }

    // 2. If RAM is exhausted, try to read from buffered sustain data
    return flash_data_buffered_reader_.read_next(out);
  }

private:
  // State for both memory and file-based sources
  uint32_t ram_read_pos_; // Current read position within the RAM attack buffer

  // State for memory-based sources (SampleData)
  const SampleData *sample_data_ptr_ = nullptr;

  // State for file-based sources
  AudioBlock attack_buffer_ram_;
  uint32_t attack_buffer_length_;

  enum class SourceType {
    NONE,
    FROM_SAMPLEDATA,
    FROM_FILE
  };
  SourceType source_type_;

  // Internal readers for handling the sustain data portion with double
  // buffering
  MemorySampleReader flash_data_memory_reader_;
  UnbufferedFileSampleReader flash_data_file_reader_;
  SustainReader sustain_reader_proxy_;
  BufferedReader<NumFlashBufferBlocks, musin::hal::PicoDmaCopier>
      flash_data_buffered_reader_;
};

} // namespace musin

#endif // MUSIN_AUDIO_ATTACK_BUFFERING_SAMPLE_READER_H_
