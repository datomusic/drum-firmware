#include "drum/config.h"
#include "drum/message_router.h"
#include "test/midi_test_support.h"
#include "test/test_support.h"

// Mocks for dependencies of MessageRouter
#include "drum/audio_engine.h"
#include "drum/sequencer_controller.h"
#include "musin/hal/logger.h"
#include "musin/timing/tempo.h"

// --- Mock Implementations ---

class MockAudioEngine : public drum::AudioEngine {
public:
  MockAudioEngine(musin::Logger &logger) : drum::AudioEngine(logger) {
  }
  void on_change(const drum::Events::NoteEvent &event) override {
  }
  void set_pitch(uint8_t track_index, float pitch) override {
  }
  void set_filter_frequency(float frequency) override {
  }
  void set_filter_resonance(float resonance) override {
  }
  void set_volume(float volume) override {
  }
  void set_crush_depth(float depth) override {
  }
  void set_crush_rate(float rate) override {
  }
};

template <size_t NumTracks, size_t NumSteps>
class MockSequencerController
    : public drum::SequencerController<NumTracks, NumSteps> {
public:
  MockSequencerController(musin::timing::Tempo &tempo, musin::Logger &logger)
      : drum::SequencerController<NumTracks, NumSteps>(tempo, logger) {
  }
  void on_change(const drum::Events::ParameterChangeEvent &event) override {
  }
  void set_active_note_for_track(uint8_t track_index, uint8_t note) override {
  }
};

class MockLogger : public musin::Logger {
public:
  void log(musin::LogLevel level, const char *format, ...) override {
    // No-op for these tests
  }
};

// --- Test Suite ---

TEST_CASE("MessageRouter MIDI Output Tests", "[message_router]") {
  using namespace drum;
  using namespace musin::midi;

  // Setup mock dependencies
  MockLogger logger;
  musin::timing::Tempo tempo;
  MockAudioEngine audio_engine(logger);
  MockSequencerController<drum::config::NUM_TRACKS,
                          drum::config::NUM_STEPS_PER_TRACK>
      sequencer_controller(tempo, logger);

  MessageRouter router(audio_engine, sequencer_controller, logger);

  SECTION("Parameter change sends MIDI CC when mode is BOTH") {
    reset_test_state();
    router.set_output_mode(OutputMode::BOTH);

    router.set_parameter(Parameter::VOLUME, 0.5f, std::nullopt);
    process_midi_output_queue();

    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
    // Volume is CC 7. 0.5f * 127 = 63.5, rounded is 64.
    REQUIRE(MIDI::internal::mock_midi_calls[0] ==
            MIDI::internal::MockMidiCallRecord::ControlChange(
                drum::config::MIDI_OUT_CHANNEL, 7, 64));
  }

  SECTION("Parameter change sends MIDI CC when mode is MIDI") {
    reset_test_state();
    router.set_output_mode(OutputMode::MIDI);

    router.set_parameter(Parameter::FILTER_FREQUENCY, 1.0f, std::nullopt);
    process_midi_output_queue();

    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
    // Filter Freq is CC 74. 1.0f * 127 = 127.
    REQUIRE(MIDI::internal::mock_midi_calls[0] ==
            MIDI::internal::MockMidiCallRecord::ControlChange(
                drum::config::MIDI_OUT_CHANNEL, 74, 127));
  }

  SECTION("Parameter change does not send MIDI CC when mode is AUDIO") {
    reset_test_state();
    router.set_output_mode(OutputMode::AUDIO);

    router.set_parameter(Parameter::VOLUME, 0.5f, std::nullopt);
    process_midi_output_queue();

    REQUIRE(MIDI::internal::mock_midi_calls.empty());
  }

  SECTION("Per-track parameter change sends correct MIDI CC") {
    reset_test_state();
    router.set_output_mode(OutputMode::MIDI);

    // Track 2 (index 1) Pitch
    router.set_parameter(Parameter::PITCH, 0.25f, 1);
    process_midi_output_queue();

    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
    // Track 2 Pitch is CC 22. 0.25f * 127 = 31.75, rounded is 32.
    REQUIRE(MIDI::internal::mock_midi_calls[0] ==
            MIDI::internal::MockMidiCallRecord::ControlChange(
                drum::config::MIDI_OUT_CHANNEL, 22, 32));
  }

  SECTION("NoteOn event sends MIDI NoteOn when mode is MIDI") {
    reset_test_state();
    router.set_output_mode(OutputMode::MIDI);

    drum::Events::NoteEvent event{
        .track_index = 0, .note = 60, .velocity = 100};
    router.notification(event);
    router.update(); // This moves event from internal queue to the main MIDI
                     // output queue
    process_midi_output_queue();

    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
    REQUIRE(MIDI::internal::mock_midi_calls[0] ==
            MIDI::internal::MockMidiCallRecord::NoteOn(
                drum::config::MIDI_OUT_CHANNEL, 60, 100));
  }

  SECTION("NoteOff event (velocity 0) sends MIDI NoteOff when mode is BOTH") {
    reset_test_state();
    router.set_output_mode(OutputMode::BOTH);

    drum::Events::NoteEvent event{.track_index = 1, .note = 62, .velocity = 0};
    router.notification(event);
    router.update();
    process_midi_output_queue();

    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
    // The wrapper turns a NoteOn with velocity 0 into a NoteOff message type
    REQUIRE(MIDI::internal::mock_midi_calls[0] ==
            MIDI::internal::MockMidiCallRecord::NoteOff(
                drum::config::MIDI_OUT_CHANNEL, 62, 0));
  }

  SECTION("NoteOn event does not send MIDI when mode is AUDIO") {
    reset_test_state();
    router.set_output_mode(OutputMode::AUDIO);

    drum::Events::NoteEvent event{
        .track_index = 0, .note = 60, .velocity = 100};
    router.notification(event);
    router.update();
    process_midi_output_queue();

    REQUIRE(MIDI::internal::mock_midi_calls.empty());
  }
}
