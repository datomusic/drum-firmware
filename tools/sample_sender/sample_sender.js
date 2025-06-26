#!/usr/bin/env node
const midi = require('midi');
const fs = require('fs');
const wavefile = require('wavefile');

function find_dato_drum(){
  const output = new midi.Output();
  const count = output.getPortCount();
  const validPortNames = ["Pico", "DRUM"]; // Array of valid port name substrings

  for (var i=0;i<count;++i) {
    const portName = output.getPortName(i);
    for (const validName of validPortNames) {
      if (portName.includes(validName)) {
        try {
          output.openPort(i);
          console.log(`Opened MIDI port: ${portName}`);
        return output;
      } catch (e) {
        console.error(`Error opening MIDI port ${output.getPortName(i)}: ${e.message}`);
        // Continue searching for other ports
      }
    } // Closes: if (portName.includes(validName))
  } // Closes: for (const validName of validPortNames)
} // Closes: for (var i=0;i<count;++i)

  console.error(`No suitable MIDI output port found containing any of: ${validPortNames.join(", ")}.`);
  if (count > 0) {
    console.log("Available MIDI output ports:");
    for (var i = 0; i < count; ++i) {
      console.log(`  ${i}: ${output.getPortName(i)}`);
    }
  } else {
    console.log("No MIDI output ports found.");
  }
  console.error("Please ensure your Dato DRUM device is connected and not in use by another application.");
  return null;
}


const activeMidiOutput = find_dato_drum();

if (!activeMidiOutput) {
  console.error("Failed to initialize MIDI output. Exiting.");
  process.exit(1);
}


function send_drum_message(tag, body) {
  try {
    activeMidiOutput.sendMessage([0xF0].concat(
      [0, 0x7D, 0x65], // Manufacturer ID
      [0, 0, tag],
      body,
      [0xF7]));
  } catch (e) {
    console.error(`Error sending MIDI message: ${e.message}`);
    // Depending on the severity, you might want to re-throw or exit
  }
}

function begin_file_transfer(source_path, file_name) {
  console.log(`Copying ${source_path} to ${file_name}`);
  const encoded = new TextEncoder().encode(file_name);

  var bytes = [];

  var i = 0;
  for (i=0;i<encoded.length;i+=2) {
    // Pack two bytes into a 16bit value, and then into 3 sysex bytes.
    const lower = encoded[i]
    const upper = encoded[i+1]
    bytes = bytes.concat(pack3_16((upper << 8) + lower));
  }

  if (encoded.length % 2 == 0) {
    bytes = bytes.concat(pack3_16(encoded[encoded.length - 1] << 8));
  }

  send_drum_message(0x10, bytes);
}

function end_file_transfer() {
  console.log("File transfer complete\n");
  send_drum_message(0x12, []);
}

function pack3_16(value) {
  return [
      (value >> 14) & 0x7F,
      (value >> 7) & 0x7F,
      value & 0x7F
  ];
}

const sleepMs = (milliseconds) => new Promise(resolve => setTimeout(resolve, milliseconds));

async function send_file_content(data) {
  console.log("File data length: ", data.length);

  var bytes = [];
  var chunk_counter = 0;

  for (var i = 0; i < data.length; i += 2) {
    // Pack two bytes into a 16bit value, and then into 3 sysex bytes.
    const lower = data[i];
    // If there's no upper byte (odd length file), use 0 as a placeholder.
    const upper = (i + 1 < data.length) ? data[i + 1] : 0;
    bytes = bytes.concat(pack3_16((upper << 8) + lower));

    // Stay below the max SysEx message length, leaving space for header.
    if (bytes.length >= 100) {
      process.stdout.write(`Sending chunk ${chunk_counter}\r`);
      chunk_counter++;
      send_drum_message(0x11, bytes);
      bytes = [];

      // Don't overload buffers of the DRUM
      await sleepMs(3);
    }
  }

  // Print a final newline to move off the counter line
  process.stdout.write('\n');

  // Send any remaining bytes that didn't fill a full chunk
  if (bytes.length > 0) {
    console.log("Sending final chunk");
    send_drum_message(0x11, bytes);
  }

  await sleepMs(100);
}


function pcm_from_wav(path) {
  const wav = new wavefile.WaveFile(data);
  wav.toSampleRate(44100)
  console.log(wav);
  return wav.data.samples
}


const source_path = process.argv[2]
const sample_filename = process.argv[3]

if (!sample_filename) {
}

if (!sample_filename || !source_path) {
  console.log("Error: Supply source file path as first argument and target on-device filename as second argument.");
  process.exit(1);
}

if (!fs.existsSync(source_path)) {
  console.error(`Error: Source file not found at '${source_path}'`);
  process.exit(1);
}

begin_file_transfer(source_path, sample_filename);

// const data = pcm_from_wav('../../experiments/support/samples/Zap_2.wav')
const data = fs.readFileSync(source_path);

send_file_content(data).then( () => {
  end_file_transfer();
  activeMidiOutput.closePort();
})
