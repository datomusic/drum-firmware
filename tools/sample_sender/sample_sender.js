#!/usr/bin/env node
const midi = require('midi');
const fs = require('fs');
const wavefile = require('wavefile');

function find_dato_drum() {
  const output = new midi.Output();
  const input = new midi.Input();
  const outPortCount = output.getPortCount();
  const inPortCount = input.getPortCount();
  const validPortNames = ["Pico", "DRUM"]; // Array of valid port name substrings

  let outPort = -1;
  let inPort = -1;
  let portName = "";

  // Find the first available output port that matches our device names
  for (let i = 0; i < outPortCount; i++) {
    const currentPortName = output.getPortName(i);
    if (validPortNames.some(name => currentPortName.includes(name))) {
      outPort = i;
      portName = currentPortName;
      break;
    }
  }

  if (outPort === -1) {
    console.error(`No suitable MIDI output port found containing any of: ${validPortNames.join(", ")}.`);
    if (outPortCount > 0) {
      console.log("Available MIDI output ports:");
      for (let i = 0; i < outPortCount; ++i) {
        console.log(`  ${i}: ${output.getPortName(i)}`);
      }
    } else {
      console.log("No MIDI output ports found.");
    }
    console.error("Please ensure your Dato DRUM device is connected and not in use by another application.");
    return null;
  }

  // Find the matching input port
  for (let i = 0; i < inPortCount; i++) {
    if (input.getPortName(i).includes(portName)) {
      inPort = i;
      break;
    }
  }

  if (inPort === -1) {
    console.error(`Found MIDI output "${portName}", but no matching input port could be found.`);
    if (inPortCount > 0) {
      console.log("Available MIDI input ports:");
      for (let i = 0; i < inPortCount; ++i) {
        console.log(`  ${i}: ${input.getPortName(i)}`);
      }
    } else {
      console.log("No MIDI input ports found.");
    }
    return null;
  }

  try {
    output.openPort(outPort);
    input.openPort(inPort);
    // Do not ignore sysex, timing, or active sensing messages.
    input.ignoreTypes(false, false, false);
    console.log(`Opened MIDI ports: ${portName}`);
    return { output, input };
  } catch (e) {
    console.error(`Error opening MIDI ports for ${portName}: ${e.message}`);
    return null;
  }
}

const midi_ports = find_dato_drum();

if (!midi_ports) {
  console.error("Failed to initialize MIDI. Exiting.");
  process.exit(1);
}

const { output: activeMidiOutput, input: activeMidiInput } = midi_ports;

// --- SysEx Configuration ---
const SYSEX_MANUFACTURER_ID = [0x00, 0x22, 0x01]; // Dato Musical Instruments
const SYSEX_DEVICE_ID = 0x65;                     // DRUM device ID

// --- ACK/NACK Handling ---
const REQUEST_FIRMWARE_VERSION = 0x01;
const ACK = 0x13;
const NACK = 0x14;
let ackPromise = {};
let replyPromise = {};

activeMidiInput.on('message', (deltaTime, message) => {
  // SysEx message: F0 ... F7
  if (message[0] === 0xF0 && message[message.length - 1] === 0xF7) {
    // Check for our manufacturer ID and device ID
    const isOurManufacturer = message[1] === SYSEX_MANUFACTURER_ID[0] &&
                            message[2] === SYSEX_MANUFACTURER_ID[1] &&
                            message[3] === SYSEX_MANUFACTURER_ID[2];

    if (isOurManufacturer && message[4] === SYSEX_DEVICE_ID) {
      // Ensure message is long enough to have a tag
      if (message.length < 6) {
        console.log("Parsed our device ID, but message is too short for a tag.");
        return;
      }
      const tag = message[5];
      // console.log(`Parsed SysEx message with tag: ${tag}`);

      if (tag === ACK) {
        // console.log("Received ACK.");
        if (ackPromise.resolve) {
          ackPromise.resolve();
          ackPromise = {};
        }
      } else if (tag === NACK) {
        // console.log("Received NACK.");
        if (ackPromise.reject) {
          ackPromise.reject(new Error('Received NACK from device.'));
          ackPromise = {};
        }
      } else {
        // console.log("Received data reply.");
        if (replyPromise.resolve) {
          replyPromise.resolve(message); // Resolve with the full message
          replyPromise = {};
        } else {
          console.log("No reply promise was waiting for this data message.");
        }
      }
    } else {
      console.log("Received SysEx message, but not for our device.");
    }
  }
});

function waitForAck(timeout = 20000) {
  return new Promise((resolve, reject) => {
    ackPromise = { resolve, reject };
    setTimeout(() => {
      if (ackPromise.reject) {
        ackPromise.reject(new Error(`Timeout waiting for ACK after ${timeout}ms.`));
        ackPromise = {};
      }
    }, timeout);
  });
}

function waitForReply(timeout = 20000) {
  return new Promise((resolve, reject) => {
    replyPromise = { resolve, reject };
    setTimeout(() => {
      if (replyPromise.reject) {
        replyPromise.reject(new Error(`Timeout waiting for reply after ${timeout}ms.`));
        replyPromise = {};
      }
    }, timeout);
  });
}

async function send_drum_message_and_wait(tag, body) {
  // The payload consists of the command tag followed by the body,
  // with the tag encoded as a 16-bit value into three 7-bit bytes.
  const payload = pack3_16(tag).concat(body);

  const message = [0xF0].concat(
    SYSEX_MANUFACTURER_ID,
    [SYSEX_DEVICE_ID],
    payload,
    [0xF7]
  );
  try {
    activeMidiOutput.sendMessage(message);
    await waitForAck();
  } catch (e) {
    console.error(`Error sending MIDI message or waiting for ACK: ${e.message}`);
    // Re-throw to be caught by the main execution block
    throw e;
  }
}

async function begin_file_transfer(source_path, file_name) {
  console.log(`Copying ${source_path} to ${file_name}`);
  const encoded = new TextEncoder().encode(file_name);
  const encoded_with_null = new Uint8Array(encoded.length + 1);
  encoded_with_null.set(encoded);
  encoded_with_null[encoded.length] = 0; // Null terminator

  var bytes = [];

  for (let i = 0; i < encoded_with_null.length; i += 2) {
    // Pack two bytes into a 16bit value, and then into 3 sysex bytes.
    const lower = encoded_with_null[i];
    const upper = (i + 1 < encoded_with_null.length) ? encoded_with_null[i + 1] : 0;
    bytes = bytes.concat(pack3_16((upper << 8) | lower));
  }

  await send_drum_message_and_wait(0x10, bytes);
}

async function end_file_transfer() {
  await send_drum_message_and_wait(0x12, []);
}

async function format_filesystem() {
  console.log("Sending command to format filesystem...");
  await send_drum_message_and_wait(0x15, []);
}

async function get_firmware_version() {
  console.log("Querying device firmware version...");
  const tag = REQUEST_FIRMWARE_VERSION;
  const message = [0xF0].concat(
    SYSEX_MANUFACTURER_ID,
    [SYSEX_DEVICE_ID],
    pack3_16(tag), // Encoded tag
    [0xF7]
  );
  try {
    activeMidiOutput.sendMessage(message);
    const reply = await waitForReply();
    // The reply for a version request has the version tag and data
    if (reply[5] === REQUEST_FIRMWARE_VERSION) {
      const major = reply[6];
      const minor = reply[7];
      const patch = reply[8];
      console.log(`Device firmware version: v${major}.${minor}.${patch}`);
    } else {
      // Unexpected reply
      throw new Error(`Unexpected reply received. Tag: ${reply[5]}`);
    }
  } catch (e) {
    console.error(`Error requesting firmware version: ${e.message}`);
    throw e;
  }
}

function pack3_16(value) {
  return [
      (value >> 14) & 0x7F,
      (value >> 7) & 0x7F,
      value & 0x7F
  ];
}

async function send_file_content(data) {
  console.log("File data length: ", data.length);

  var bytes = [];
  const total_bytes = data.length;

  for (var i = 0; i < data.length; i += 2) {
    // Pack two bytes into a 16bit value, and then into 3 sysex bytes.
    const lower = data[i];
    // If there's no upper byte (odd length file), use 0 as a placeholder.
    const upper = (i + 1 < data.length) ? data[i + 1] : 0;
    bytes = bytes.concat(pack3_16((upper << 8) + lower));

    // Stay below the max SysEx message length, leaving space for header.
    if (bytes.length >= 100) {
      const percentage = Math.round(((i + 2) / total_bytes) * 100);
      const bar_length = 40;
      const filled_length = Math.round(bar_length * (percentage / 100));
      const bar = '█'.repeat(filled_length) + '░'.repeat(bar_length - filled_length);
      process.stdout.write(`Sending: ${percentage}% |${bar}| \r`);

      await send_drum_message_and_wait(0x11, bytes);
      bytes = [];
    }
  }

  // Send any remaining bytes that didn't fill a full chunk
  if (bytes.length > 0) {
    await send_drum_message_and_wait(0x11, bytes);
  }

  // Finalize progress bar at 100%
  const bar_length = 40;
  const bar = '█'.repeat(bar_length);
  process.stdout.write(`Sending: 100% |${bar}| \n`);
}


function pcm_from_wav(path) {
  const wav = new wavefile.WaveFile(data);
  wav.toSampleRate(44100)
  console.log(wav);
  return wav.data.samples
}


const command = process.argv[2];
const source_path = process.argv[3];
const sample_filename = process.argv[4];

if (!command) {
  console.log("Error: Please specify a command: 'send' or 'format'.");
  console.log("Usage:");
  console.log("  tools/sample_sender/sample_sender.js send <source_path> <target_filename>");
  console.log("  tools/sample_sender/sample_sender.js format");
  process.exit(1);
}

async function main() {
  try {
    await get_firmware_version();

    if (command === 'format') {
      await format_filesystem();
      console.log("Successfully sent format command. The device will now re-initialize its filesystem.");
    } else if (command === 'send') {
      if (!sample_filename || !source_path) {
        console.log("Error: 'send' command requires a source path and target filename.");
        console.log("Usage: tools/sample_sender/sample_sender.js send <source_path> <target_filename>");
        process.exit(1);
      }
      if (!fs.existsSync(source_path)) {
        console.error(`Error: Source file not found at '${source_path}'`);
        process.exit(1);
      }

      await begin_file_transfer(source_path, sample_filename);
      const data = fs.readFileSync(source_path);
      await send_file_content(data);
      await end_file_transfer();
      console.log("Successfully transferred file.");
    } else {
      console.log(`Error: Unknown command '${command}'. Use 'send' or 'format'.`);
      process.exit(1);
    }
  } catch (e) {
    // Error is already logged by the function that threw it.
    // Just ensure we exit with an error code.
    process.exitCode = 1;
  } finally {
    activeMidiOutput.closePort();
    activeMidiInput.closePort();
  }
}

main();
