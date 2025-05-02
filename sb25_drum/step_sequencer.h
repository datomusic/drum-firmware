#ifndef STEP_SEQUENCER_H
#define STEP_SEQUENCER_H

#include "etl/array.h"
#include <cstdint>
#include <optional>

namespace StepSequencer {

/**
 * @brief Represents a single step in a sequencer track.
 */
struct Step {
  std::optional<uint8_t> note = std::nullopt;     // MIDI note number (0-127)
  std::optional<uint8_t> velocity = std::nullopt; // MIDI velocity (1-127)
  bool enabled = false;

  constexpr Step() = default;
  constexpr Step(std::optional<uint8_t> n, std::optional<uint8_t> v, bool en)
      : note(n), velocity(v), enabled(en) {
  }
};

/**
 * @brief Represents a single track in the sequencer.
 * @tparam NumSteps The number of steps in this track.
 */
template <size_t NumSteps> class Track {
public:
  static_assert(NumSteps > 0, "Track must have at least one step.");

  constexpr Track() = default;

  /**
   * @brief Get a reference to a specific step.
   * @param index The index of the step (0 to NumSteps - 1).
   * @return A reference to the Step object.
   * @note Assumes index is valid.
   */
  [[nodiscard]] constexpr Step &get_step(size_t index) {
    return steps[index];
  }

  /**
   * @brief Get a const reference to a specific step.
   * @param index The index of the step (0 to NumSteps - 1).
   * @return A const reference to the Step object.
   * @note Assumes index is valid.
   */
  [[nodiscard]] constexpr const Step &get_step(size_t index) const {
    // Basic bounds check (can be enhanced with ETL assertions if desired)
    ETL_ASSERT(index < NumSteps, etl::range_error("Track::get_step: index out of bounds"));
    return steps[index];
  }


  /**
   * @brief Get the total number of steps in this track.
   */
  [[nodiscard]] constexpr size_t size() const {
    return NumSteps;
  }

  /**
   * @brief Toggles the enabled state of a specific step.
   * @param step_idx The index of the step (0 to NumSteps-1).
   * @return The new enabled state of the step.
   */
  constexpr bool toggle_step_enabled(size_t step_idx) {
    ETL_ASSERT(step_idx < NumSteps, etl::range_error("Track::toggle_step_enabled: index out of bounds"));
    steps[step_idx].enabled = !steps[step_idx].enabled;
    return steps[step_idx].enabled;
  }

  /**
   * @brief Sets the note for a specific step.
   * @param step_idx The index of the step (0 to NumSteps-1).
   * @param note The MIDI note number (0-127) or std::nullopt.
   */
  constexpr void set_step_note(size_t step_idx, std::optional<uint8_t> note) {
    ETL_ASSERT(step_idx < NumSteps, etl::range_error("Track::set_step_note: index out of bounds"));
    steps[step_idx].note = note;
  }

  /**
   * @brief Sets the velocity for a specific step.
   * @param step_idx The index of the step (0 to NumSteps-1).
   * @param velocity The MIDI velocity (1-127) or std::nullopt.
   */
  constexpr void set_step_velocity(size_t step_idx, std::optional<uint8_t> velocity) {
    ETL_ASSERT(step_idx < NumSteps, etl::range_error("Track::set_step_velocity: index out of bounds"));
    steps[step_idx].velocity = velocity;
  }

  /**
   * @brief Gets the velocity of a specific step.
   * @param step_idx The index of the step (0 to NumSteps-1).
   * @return std::optional<uint8_t> containing the velocity if set.
   */
  [[nodiscard]] constexpr std::optional<uint8_t> get_step_velocity(size_t step_idx) const {
    ETL_ASSERT(step_idx < NumSteps, etl::range_error("Track::get_step_velocity: index out of bounds"));
    return steps[step_idx].velocity;
  }

  /**
   * @brief Sets the note value for all steps in the track.
   * @param note_value The MIDI note number (0-127).
   */
  constexpr void set_note(uint8_t note_value) {
    for (size_t i = 0; i < NumSteps; ++i) {
      steps[i].note = note_value;
    }
  }


private:
  etl::array<Step, NumSteps> steps;
};

/**
 * @brief Represents the main sequencer engine.
 * @tparam NumTracks The number of tracks in the sequencer.
 * @tparam NumSteps The number of steps per track.
 */
template <size_t NumTracks, size_t NumSteps> class Sequencer {
public:
  static_assert(NumTracks > 0 && NumSteps > 0,
                "Sequencer must have at least one track and one step");

  constexpr Sequencer() = default;

  /**
   * @brief Get a reference to a specific track.
   * @param index The index of the track (0 to NumTracks - 1).
   * @return A reference to the Track object.
   * @note Assumes index is valid.
   */
  [[nodiscard]] constexpr Track<NumSteps> &get_track(size_t index) {
    return tracks[index];
  }

  /**
   * @brief Get a const reference to a specific track.
   * @param index The index of the track (0 to NumTracks - 1).
   * @return A const reference to the Track object.
   * @note Assumes index is valid.
   */
  [[nodiscard]] constexpr const Track<NumSteps> &get_track(size_t index) const {
    return tracks[index];
  }

  /**
   * @brief Get the total number of tracks in the sequencer.
   */
  [[nodiscard]] constexpr size_t get_num_tracks() const {
    return NumTracks;
  }

  /**
   * @brief Get the number of steps per track.
   */
  [[nodiscard]] constexpr size_t get_num_steps() const {
    return NumSteps;
  }

private:
  etl::array<Track<NumSteps>, NumTracks> tracks;
};

} // namespace StepSequencer

#endif // STEP_SEQUENCER_H
