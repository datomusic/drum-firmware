#include "sequencer_persister.h"
#include <cstdio>

namespace drum {

bool SequencerPersister::save_to_file(const char *filepath,
                                      const SequencerPersistentState &state) {
  FILE *file = fopen(filepath, "wb");
  if (!file) {
    return false;
  }

  size_t written = fwrite(&state, sizeof(SequencerPersistentState), 1, file);
  fclose(file);

  return written == 1;
}

bool SequencerPersister::load_from_file(const char *filepath,
                                        SequencerPersistentState &state) {
  FILE *file = fopen(filepath, "rb");
  if (!file) {
    return false; // File doesn't exist, not an error
  }

  size_t read_size = fread(&state, sizeof(SequencerPersistentState), 1, file);
  fclose(file);

  if (read_size != 1 || !state.is_valid()) {
    return false; // Corrupted or invalid file
  }

  return true;
}

} // namespace drum
