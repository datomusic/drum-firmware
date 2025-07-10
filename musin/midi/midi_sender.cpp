#include "midi_sender.h"
#include "musin/midi/midi_wrapper.h" // For MIDI::internal actual send functions and MIDI::send functions
#include "musin/midi/midi_output_queue.h" // For enqueue_midi_message

namespace musin::midi {

MidiSender::MidiSender(MidiSendStrategy strategy, musin::Logger& logger)
    : _strategy(strategy), _logger(logger) {
}

void MidiSender::sendNoteOn(uint8_t channel, uint8_t note_number, uint8_t velocity) {
    if (_strategy == MidiSendStrategy::DIRECT_BYPASS_QUEUE) {
        _logger.info("MIDI_SENDER: Direct NoteOn");
        MIDI::internal::_sendNoteOn_actual(channel, note_number, velocity);
    } else {
        _logger.info("MIDI_SENDER: Queued NoteOn");
        enqueue_midi_message(OutgoingMidiMessage(channel, note_number, velocity, true), _logger);
    }
}

void MidiSender::sendNoteOff(uint8_t channel, uint8_t note_number, uint8_t velocity) {
    if (_strategy == MidiSendStrategy::DIRECT_BYPASS_QUEUE) {
        _logger.info("MIDI_SENDER: Direct NoteOff");
        MIDI::internal::_sendNoteOff_actual(channel, note_number, velocity);
    } else {
        _logger.info("MIDI_SENDER: Queued NoteOff");
        enqueue_midi_message(OutgoingMidiMessage(channel, note_number, velocity, false), _logger);
    }
}

void MidiSender::sendControlChange(uint8_t channel, uint8_t controller, uint8_t value) {
    if (_strategy == MidiSendStrategy::DIRECT_BYPASS_QUEUE) {
        _logger.info("MIDI_SENDER: Direct ControlChange");
        MIDI::internal::_sendControlChange_actual(channel, controller, value);
    } else {
        _logger.info("MIDI_SENDER: Queued ControlChange");
        enqueue_midi_message(OutgoingMidiMessage(channel, controller, value), _logger);
    }
}

} // namespace musin::midi
