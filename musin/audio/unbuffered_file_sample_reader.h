#ifndef MUSIN_AUDIO_UNBUFFERED_FILE_SAMPLE_READER_H_
#define MUSIN_AUDIO_UNBUFFERED_FILE_SAMPLE_READER_H_

#include "musin/audio/sample_reader.h"
#include "etl/string_view.h"
#include <stdio.h>

namespace musin {

/**
 * @brief A SampleReader that reads audio data directly from the filesystem.
 *
 * This reader is unbuffered and performs file I/O operations on each read call.
 * It is designed to be wrapped by a component that handles buffering, such as
 * musin::BufferedReader.
 */
class UnbufferedFileSampleReader : public SampleReader {
public:
  UnbufferedFileSampleReader();
  ~UnbufferedFileSampleReader() override;

  // Non-copyable and non-movable
  UnbufferedFileSampleReader(const UnbufferedFileSampleReader&) = delete;
  UnbufferedFileSampleReader& operator=(const UnbufferedFileSampleReader&) = delete;
  UnbufferedFileSampleReader(UnbufferedFileSampleReader&&) = delete;
  UnbufferedFileSampleReader& operator=(UnbufferedFileSampleReader&&) = delete;

  /**
   * @brief Opens a file for reading.
   *
   * If a file is already open, it will be closed first.
   * @param path The path to the audio sample file.
   * @return true if the file was opened successfully, false otherwise.
   */
  bool open(const etl::string_view& path);

  /**
   * @brief Closes the currently open file, if any.
   */
  void close();

  // --- SampleReader Interface ---
  void reset() override;
  bool has_data() override;
  uint32_t read_samples(AudioBlock& out) override;
  bool read_next(int16_t& out) override;

private:
  FILE* file_handle_ = nullptr;
};

} // namespace musin

#endif // MUSIN_AUDIO_UNBUFFERED_FILE_SAMPLE_READER_H_
