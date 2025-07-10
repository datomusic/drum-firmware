#ifndef MUSIN_MIDI_MIDI_SENDER_H
#define MUSIN_MIDI_MIDI_SENDER_H

#include "musin/hal/logger.h" // Assuming logger is needed for debug output
#include <cstdint>

namespace musin::midi {

enum class MidiSendStrategy {
  QUEUED,
  DIRECT_BYPASS_QUEUE
};

class MidiSender {
public:
  explicit MidiSender(MidiSendStrategy strategy, musin::Logger &logger);

  void sendNoteOn(uint8_t channel, uint8_t note_number, uint8_t velocity);
  void sendNoteOff(uint8_t channel, uint8_t note_number, uint8_t velocity);
  void sendControlChange(uint8_t channel, uint8_t controller, uint8_t value);
  // Add other MIDI message types as needed (PitchBend, SysEx, RealTime)

private:
  MidiSendStrategy _strategy;
  musin::Logger &_logger;
};

} // namespace musin::midi

#endif // MUSIN_MIDI_MIDI_SENDER_H