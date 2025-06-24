#include "drum/sample_repository.h"

#include <cstdio>
#include <cstdlib> // For atoi
#include <cstring> // For strchr

namespace drum {

void SampleRepository::load_from_manifest() {
  // Clear any existing paths before loading.
  for (auto &path_opt : sample_paths_) {
    path_opt.reset();
  }

  FILE *manifest_file = fopen(MANIFEST_PATH, "r");
  if (!manifest_file) {
    // printf("Could not open sample manifest: %s. No samples loaded.\n", MANIFEST_PATH);
    return;
  }

  // A buffer to read one line of the manifest at a time.
  // The path is max 64, plus "NN: " prefix and newline/null terminator.
  char line_buffer[MAX_PATH_LENGTH + 10];

  while (fgets(line_buffer, sizeof(line_buffer), manifest_file) != nullptr) {
    // Find the colon separating the index from the path.
    char *colon_pos = strchr(line_buffer, ':');
    if (!colon_pos) {
      continue; // Invalid line format.
    }

    // Temporarily null-terminate at the colon to parse the index.
    *colon_pos = '\0';
    int index = atoi(line_buffer);

    // Restore the colon to find the path.
    *colon_pos = ':';

    if (index < 0 || static_cast<size_t>(index) >= MAX_SAMPLES) {
      continue; // Index out of bounds.
    }

    // Find the start of the path (skip whitespace after the colon).
    char *path_start = colon_pos + 1;
    while (*path_start == ' ' || *path_start == '\t') {
      path_start++;
    }

    // Find the end of the path (strip newline characters).
    char *path_end = strchr(path_start, '\n');
    if (path_end) {
      *path_end = '\0';
    }
    path_end = strchr(path_start, '\r');
    if (path_end) {
      *path_end = '\0';
    }

    // Store the path if the string is not empty.
    if (strlen(path_start) > 0) {
      sample_paths_[index].emplace(path_start);
    }
  }

  fclose(manifest_file);
}

etl::optional<etl::string_view> SampleRepository::get_path(size_t index) const {
  if (index >= MAX_SAMPLES || !sample_paths_[index].has_value()) {
    return etl::nullopt;
  }
  return etl::string_view(sample_paths_[index].value());
}

} // namespace drum
