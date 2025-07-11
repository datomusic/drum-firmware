#ifndef DRUM_PARAMETER_HANDLERS_H
#define DRUM_PARAMETER_HANDLERS_H

#include "config.h"
#include "events.h"
#include <optional>

// Forward declarations to avoid circular dependencies
namespace musin::timing {
class TempoHandler;
}
namespace drum {
class MessageRouter;
template <size_t NumTracks, size_t NumSteps> class SequencerController;
using DefaultSequencerController =
    SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>;
} // namespace drum

namespace drum {

// The "Strategy" interface
class IParameterHandler {
public:
  virtual ~IParameterHandler() = default;
  virtual void handle_update(float normalized_value) = 0;
};

// --- Concrete Handler Implementations ---

class TempoParameterHandler : public IParameterHandler {
public:
  TempoParameterHandler(musin::timing::TempoHandler &tempo_handler,
                        MessageRouter &message_router);
  void handle_update(float normalized_value) override;

private:
  musin::timing::TempoHandler &tempo_handler_;
  MessageRouter &message_router_;
};

class PitchParameterHandler : public IParameterHandler {
public:
  PitchParameterHandler(MessageRouter &message_router, uint8_t track_index);
  void handle_update(float normalized_value) override;

private:
  MessageRouter &message_router_;
  uint8_t track_index_;
};

class VolumeParameterHandler : public IParameterHandler {
public:
  explicit VolumeParameterHandler(MessageRouter &message_router);
  void handle_update(float normalized_value) override;

private:
  MessageRouter &message_router_;
};

class SwingParameterHandler : public IParameterHandler {
public:
  SwingParameterHandler(DefaultSequencerController &sequencer_controller,
                        MessageRouter &message_router);
  void handle_update(float normalized_value) override;

private:
  DefaultSequencerController &sequencer_controller_;
  MessageRouter &message_router_;
};

class CrushParameterHandler : public IParameterHandler {
public:
  explicit CrushParameterHandler(MessageRouter &message_router);
  void handle_update(float normalized_value) override;

private:
  MessageRouter &message_router_;
};

class RandomParameterHandler : public IParameterHandler {
public:
  RandomParameterHandler(DefaultSequencerController &sequencer_controller,
                         MessageRouter &message_router);
  void handle_update(float normalized_value) override;

private:
  DefaultSequencerController &sequencer_controller_;
  MessageRouter &message_router_;
};

class RepeatParameterHandler : public IParameterHandler {
public:
  RepeatParameterHandler(DefaultSequencerController &sequencer_controller,
                         MessageRouter &message_router);
  void handle_update(float normalized_value) override;

private:
  DefaultSequencerController &sequencer_controller_;
  MessageRouter &message_router_;
};

class FilterParameterHandler : public IParameterHandler {
public:
  explicit FilterParameterHandler(float &target_value);
  void handle_update(float normalized_value) override;

private:
  float &target_value_;
};

} // namespace drum

#endif // DRUM_PARAMETER_HANDLERS_H
