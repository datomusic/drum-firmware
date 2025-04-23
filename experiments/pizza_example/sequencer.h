#ifndef PIZZA_SEQUENCER_H
#define PIZZA_SEQUENCER_H

#include "etl/array.h"
#include <cstdint>
#include <optional>

namespace PizzaSequencer {

/**
 * @brief Represents a single step in a sequencer track.
 */
struct Step {
    std::optional<uint8_t> note = std::nullopt; // MIDI note number (0-127)
    std::optional<uint8_t> velocity = std::nullopt; // MIDI velocity (1-127)
    bool enabled = false;

    constexpr Step() = default;
    constexpr Step(std::optional<uint8_t> n, std::optional<uint8_t> v, bool en) : note(n), velocity(v), enabled(en) {}
};

/**
 * @brief Represents a single track in the sequencer.
 * @tparam NumSteps The number of steps in this track.
 */
template <size_t NumSteps>
class Track {
public:
    static_assert(NumSteps > 0, "Track must have at least one step.");

    constexpr Track() = default;

    /**
     * @brief Get a reference to a specific step.
     * @param index The index of the step (0 to NumSteps - 1).
     * @return A reference to the Step object.
     * @note Assumes index is valid. Add bounds checking (e.g., assert) if needed.
     */
    [[nodiscard]] constexpr Step& get_step(size_t index) {
        // hard_assert(index < NumSteps);
        return steps[index];
    }

    /**
     * @brief Get a const reference to a specific step.
     * @param index The index of the step (0 to NumSteps - 1).
     * @return A const reference to the Step object.
     * @note Assumes index is valid. Add bounds checking (e.g., assert) if needed.
     */
    [[nodiscard]] constexpr const Step& get_step(size_t index) const {
        // hard_assert(index < NumSteps);
        return steps[index];
    }

    /**
     * @brief Get the total number of steps in this track.
     */
    [[nodiscard]] constexpr size_t size() const {
        return NumSteps;
    }

    /**
     * @brief Set the note number for all steps in this track.
     * @param note_value The MIDI note number (0-127) to set for all steps.
     */
    void set_all_notes(uint8_t note_value) {
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
template <size_t NumTracks, size_t NumSteps>
class Sequencer {
public:
    static_assert(NumTracks > 0, "Sequencer must have at least one track.");

    constexpr Sequencer() = default;

    /**
     * @brief Get a reference to a specific track.
     * @param index The index of the track (0 to NumTracks - 1).
     * @return A reference to the Track object.
     * @note Assumes index is valid. Add bounds checking (e.g., assert) if needed.
     */
    [[nodiscard]] constexpr Track<NumSteps>& get_track(size_t index) {
        // hard_assert(index < NumTracks);
        return tracks[index];
    }

    /**
     * @brief Get a const reference to a specific track.
     * @param index The index of the track (0 to NumTracks - 1).
     * @return A const reference to the Track object.
     * @note Assumes index is valid. Add bounds checking (e.g., assert) if needed.
     */
    [[nodiscard]] constexpr const Track<NumSteps>& get_track(size_t index) const {
        // hard_assert(index < NumTracks);
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
    // Add tempo, current step, etc. later
};

} // namespace PizzaSequencer

#endif // PIZZA_SEQUENCER_H
