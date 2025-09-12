#include "musin/hal/null_logger.h"
#include "musin/midi/midi_output_queue.h"
#include "musin/midi/midi_wrapper.h"

namespace MIDI {

void sendRealTime(const ::midi::MidiType message) {
  musin::midi::OutgoingMidiMessage msg(message);
  static musin::NullLogger logger;
  musin::midi::enqueue_midi_message(msg, logger);
}

} // namespace MIDI
