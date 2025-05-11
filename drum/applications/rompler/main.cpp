#include <pico/stdio_usb.h>
#include <pico/time.h>

#include "etl/vector.h"

#include "musin/midi/midi_wrapper.h"
#include "musin/usb/usb.h"

#include <stdio.h>

#include "musin/audio/audio_output.h"
#include "rompler.h"

// File receiving:
// - React to some file-transfer start event. SysEx message or something else.
// - Probably enter some file-transfer state, audio, sequencer etc. are disabled.
// - Open a source stream from the relevant transport (sysex, serial, whatever).
// - Notify sink (saving to filesystem) about start of a new transfer. Essentially opens a file.
// - Decode incoming data into bytes
// - Keep two buffers
// - Read into one buffer until full, or stream ends
// - Switch buffers, and read following bytes into second one
// - Pass the filled buffer to sink (which will write data to file)
// - If other buffer is filled

struct SysexParser {
  typedef etl::vector<uint8_t, MIDI::SysExMaxSize> Chunk;

  void parse(const Chunk &chunk) {
    if (state == State::Idle) {
      if (chunk.size() > 3 && chunk[1] == DatoId && chunk[2] == DrumId) {
        state = State::Identified;
        parse_part(chunk.cbegin() + 3, chunk.cend());
      }
    } else {
      parse_part(chunk.cbegin(), chunk.cend());
    }
  }

private:
  static const uint8_t DatoId = 0x7D; // Manufacturer ID for Dato
  static const uint8_t DrumId = 0x65; // Device ID for DRUM

  enum Protocol {
    BeginFileTransfer = 0x08,
    EndFileTransfer = 0x09
  };

  void parse_part(Chunk::const_iterator iterator, const Chunk::const_iterator end) {
    if (iterator == end) {
      return;
    }

    switch (state) {
    case State::Identified: {
      const auto tag = *iterator;
      iterator++;

      if (tag == BeginFileTransfer) {
        state = State::FileTransfer;
        parse_part(iterator, end);
      }

    } break;

    case State::FileTransfer: {
      // TODO: - Decode sysex messages to bytes
      //       - Pass them to some handler based on type
      //       - Currently only one type of file transfer (samples)
    }

    break;

    case State::Idle:
      // TODO: Unexpected situation. Log it somehow.
      break;
    }
  }

  enum class State {
    Idle,
    Identified,
    FileTransfer,
  };

  State state = State::Idle;
};

static SysexParser syx_parser;

static void handle_sysex(byte *data, unsigned length) {
  const SysexParser::Chunk chunk(data, data + length);
  syx_parser.parse(chunk);
}

int main() {
  stdio_usb_init();
  musin::usb::init();

  for (int i = 0; i < 80; ++i) {
    sleep_ms(100);
    printf(".\n");
  }

  SampleBank bank;
  Rompler rompler(bank);

  MIDI::init(MIDI::Callbacks{.sysex = handle_sysex});

  printf("Initializing audio output\n");
  if (!AudioOutput::init()) {
    printf("Audio initialization failed\n");
    return 1;
  }

  printf("Starting main loop\n");
  while (true) {
    musin::usb::background_update();
    sleep_ms(1);
  }
  return 0;
}
