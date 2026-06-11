#ifndef DRUM_SEQUENCER_PERSISTENCE_MANAGER_H
#define DRUM_SEQUENCER_PERSISTENCE_MANAGER_H

#include "musin/hal/logger.h"
#include "sequencer_persistence.h"
#include "sequencer_storage.h"
#include <cstddef>
#include <optional>

namespace drum {

/**
 * @brief Owns the optional flash storage lifecycle for sequencer state.
 *
 * Storage cannot exist until the filesystem is ready, so it starts empty and
 * is created by init(). All methods are safe no-ops (returning false) before
 * initialization, which keeps callers free of has_value() checks.
 */
template <size_t NumTracks, size_t NumSteps> class SequencerPersistenceManager {
public:
  explicit SequencerPersistenceManager(musin::Logger &logger)
      : logger_(logger) {
  }

  /**
   * @brief Create the storage backend. Call after the filesystem is ready.
   * @return false if already initialized.
   */
  bool init() {
    if (storage_.has_value()) {
      logger_.warn("Persistence already initialized");
      return false;
    }
    storage_.emplace();
    return true;
  }

  [[nodiscard]] bool is_initialized() const {
    return storage_.has_value();
  }

  void mark_dirty() {
    if (storage_.has_value()) {
      storage_->mark_state_dirty();
    }
  }

  /**
   * @brief Whether the debounced periodic save is due.
   */
  [[nodiscard]] bool should_save_now() const {
    return storage_.has_value() && storage_->should_save_now();
  }

  bool save(const SequencerPersistentState &state) {
    if (!storage_.has_value()) {
      logger_.error("Save to flash failed - persistence not initialized");
      return false;
    }
    return storage_->save_state_to_flash(state);
  }

  bool load(SequencerPersistentState &state) {
    if (!storage_.has_value()) {
      logger_.error("Load from flash failed - persistence not initialized");
      return false;
    }
    return storage_->load_state_from_flash(state);
  }

private:
  std::optional<SequencerStorage<NumTracks, NumSteps>> storage_;
  musin::Logger &logger_;
};

} // namespace drum

#endif // DRUM_SEQUENCER_PERSISTENCE_MANAGER_H
