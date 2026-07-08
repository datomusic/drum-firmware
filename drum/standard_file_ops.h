#ifndef PRINTING_FILE_OPS_H_CVXHHJDO
#define PRINTING_FILE_OPS_H_CVXHHJDO

#include "etl/optional.h"
#include "etl/span.h"
#include "etl/string_view.h"
#include "etl/utility.h"
#include "musin/filesystem/filesystem.h"
#include "musin/hal/logger.h"

#include <stdio.h>

struct StandardFileOps {
  explicit StandardFileOps(musin::Logger &logger,
                           musin::filesystem::Filesystem &filesystem)
      : logger(logger), filesystem_(filesystem) {
  }
  static const unsigned BlockSize = 256;

  struct Handle {
    // Non-copyable but movable RAII wrapper for file handles
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;

    // Move constructor
    Handle(Handle &&other) noexcept
        : logger(other.logger), file_pointer(other.file_pointer) {
      other.file_pointer = nullptr; // Transfer ownership
    }

    // Move assignment operator
    Handle &operator=(Handle &&other) noexcept {
      if (this != &other) {
        close(); // Close current file if any
        file_pointer = other.file_pointer;
        other.file_pointer = nullptr; // Transfer ownership
      }
      return *this;
    }

    Handle(const etl::string_view &path, musin::Logger &logger)
        : logger(logger) {
      if (file_pointer) {
        fclose(file_pointer);
        // TODO: Report some error. This should not happen;
      }

      logger.info("Writing file:");
      logger.info(path);
      file_pointer = fopen(path.data(), "wb");
      if (!file_pointer) {
        logger.error("Failed opening file");
      }
    }

    ~Handle() {
      close();
    }

    void close() {
      logger.info("Closing file!");
      if (file_pointer) {
        fflush(file_pointer);
        fclose(file_pointer);
        file_pointer = nullptr;
      } else {
        // TODO: Error closing a non-existent handle
      }
      return;
    }

    size_t write(const etl::span<const uint8_t> &bytes) {
      // printf("Writing %i bytes\n", bytes.size());
      if (file_pointer) {
        const auto written =
            fwrite(bytes.cbegin(), sizeof(uint8_t), bytes.size(), file_pointer);
        // printf("written: %i\n", written);
        return written;
      } else {
        // TODO: Error writing to a handle that should be exist.
        return 0;
      }
    }

  private:
    musin::Logger &logger;
    FILE *file_pointer = nullptr;
  };

  struct ReadHandle {
    ReadHandle(const ReadHandle &) = delete;
    ReadHandle &operator=(const ReadHandle &) = delete;

    ReadHandle(ReadHandle &&other) noexcept
        : file_pointer(other.file_pointer), size_(other.size_) {
      other.file_pointer = nullptr;
    }

    ReadHandle &operator=(ReadHandle &&other) noexcept {
      if (this != &other) {
        close();
        file_pointer = other.file_pointer;
        size_ = other.size_;
        other.file_pointer = nullptr;
      }
      return *this;
    }

    explicit ReadHandle(const etl::string_view &path) {
      file_pointer = fopen(path.data(), "rb");
      if (file_pointer) {
        if (fseek(file_pointer, 0, SEEK_END) == 0) {
          const long end = ftell(file_pointer);
          size_ = end > 0 ? static_cast<size_t>(end) : 0;
        }
        fseek(file_pointer, 0, SEEK_SET);
      }
    }

    ~ReadHandle() {
      close();
    }

    void close() {
      if (file_pointer) {
        fclose(file_pointer);
        file_pointer = nullptr;
      }
    }

    bool is_valid() const {
      return file_pointer != nullptr;
    }

    size_t size() const {
      return size_;
    }

    size_t read(const etl::span<uint8_t> &bytes) {
      if (!file_pointer) {
        return 0;
      }
      return fread(bytes.data(), sizeof(uint8_t), bytes.size(), file_pointer);
    }

  private:
    FILE *file_pointer = nullptr;
    size_t size_ = 0;
  };

  etl::optional<ReadHandle> open_read(const etl::string_view &path) {
    logger.info("Opening file for reading:");
    logger.info(path);
    ReadHandle handle(path);
    if (!handle.is_valid()) {
      logger.warn("Failed opening file for reading");
      return etl::nullopt;
    }
    return etl::optional<ReadHandle>(etl::move(handle));
  }

  // Handle should close upon destruction
  // TODO: Return optional instead, if handle could not be opened.
  Handle open(const etl::string_view &path) {
    // TODO: Use actual path
    // const char *path = "/tmp_sample";
    logger.info("Opening new file:");
    logger.info(path);
    return Handle(path, logger);
  }

  bool format() {
    logger.info("Formatting filesystem...");
    bool success = filesystem_.format();
    if (success) {
      logger.info("Filesystem formatted successfully.");
    } else {
      logger.error("Filesystem formatting failed.");
    }
    return success;
  }

private:
  musin::Logger &logger;
  musin::filesystem::Filesystem &filesystem_;
};

#endif /* end of include guard: PRINTING_FILE_OPS_H_CVXHHJDO */
