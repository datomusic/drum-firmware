#ifndef PRINTING_FILE_OPS_H_CVXHHJDO
#define PRINTING_FILE_OPS_H_CVXHHJDO

#include <stdio.h>

#include "etl/span.h"
#include "etl/string_view.h"
#include "musin/filesystem/filesystem.h"

struct StandardFileOps {
  static const unsigned BlockSize = 256;

  struct Handle {

    Handle(const etl::string_view &path) {
      if (file_pointer) {
        fclose(file_pointer);
        // TODO: Report some error. This should not happen;
      }

      printf("Writing file: '%s'\n", path.data());
      file_pointer = fopen(path.data(), "wb");
      if (!file_pointer) {
        printf("ERROR: Failed opening file\n");
      }
    }

    void close() {
      printf("Closing file!\n");
      if (file_pointer) {
        fflush(file_pointer);
        fclose(file_pointer);
        file_pointer = nullptr;
      } else {
        // TODO: Error closing a non-existent handle
      }
      return;
    }

    // TODO: Use Chunk instead
    size_t write(const etl::span<const uint8_t> &bytes) {
      // printf("Writing %i bytes\n", bytes.size());
      if (file_pointer) {
        const auto written = fwrite(bytes.cbegin(), sizeof(uint8_t), bytes.size(), file_pointer);
        // printf("written: %i\n", written);
        return written;
      } else {
        // TODO: Error writing to a handle that should be exist.
        return 0;
      }
    }

  private:
    FILE *file_pointer = nullptr;
  };

  // Handle should close upon destruction
  // TODO: Return optional instead, if handle could not be opened.
  Handle open(const etl::string_view &path) {
    // TODO: Use actual path
    // const char *path = "/tmp_sample";
    printf("Opening new file: %s\n", path.data());
    return Handle(path);
  }

  bool format() {
    printf("Formatting filesystem...\n");
    // This will re-initialize the filesystem, which includes formatting if the flag is true.
    bool success = musin::filesystem::init(true);
    if (success) {
      printf("Filesystem formatted successfully.\n");
    } else {
      printf("ERROR: Filesystem formatting failed.\n");
    }
    return success;
  }
};

#endif /* end of include guard: PRINTING_FILE_OPS_H_CVXHHJDO */
