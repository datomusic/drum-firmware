#ifndef PRINTING_FILE_OPS_H_CVXHHJDO
#define PRINTING_FILE_OPS_H_CVXHHJDO

#include "etl/span.h"
#include "etl/string_view.h"
#include "musin/filesystem/filesystem.h"
#include "musin/hal/logger.h"

#include <stdio.h>

struct StandardFileOps {
  explicit StandardFileOps(musin::Logger &logger, musin::filesystem::Filesystem &filesystem) 
    : logger(logger), filesystem_(filesystem) {
  }
  static const unsigned BlockSize = 256;

  struct Handle {

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
    // This will re-initialize the filesystem, which includes formatting if the
    // flag is true.
    bool success = filesystem_.init(true);
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
