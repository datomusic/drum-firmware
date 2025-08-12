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

let transferInProgress = false;

// --- SysEx Configuration ---
const SYSEX_MANUFACTURER_ID = [0x00, 0x22, 0x01]; // Dato Musical Instruments
const SYSEX_DEVICE_ID = 0x65;                     // DRUM device ID

// --- ACK/NACK Handling ---
const REQUEST_FIRMWARE_VERSION = 0x01;
const BEGIN_FILE_WRITE = 0x10;
const FILE_BYTES = 0x11;
const END_FILE_TRANSFER = 0x12;
const ACK = 0x13;
const NACK = 0x14;
const FORMAT_FILESYSTEM = 0x15;

let ackQueue = [];
let replyPromise = {};

activeMidiInput.on('message', (deltaTime, message) => {
  if (message[0] === 0xF0 && message[message.length - 1] === 0xF7) {
    const isOurManufacturer = message[1] === SYSEX_MANUFACTURER_ID[0] &&
                            message[2] === SYSEX_MANUFACTURER_ID[1] &&
                            message[3] === SYSEX_MANUFACTURER_ID[2];

    if (isOurManufacturer && message[4] === SYSEX_DEVICE_ID) {
      if (message.length < 6) return;
      const tag = message[5];

      if (tag === ACK) {
        const ackResolver = ackQueue.shift();
        if (ackResolver) {
          clearTimeout(ackResolver.timer);
          ackResolver.resolve();
        }
      } else if (tag === NACK) {
        const ackResolver = ackQueue.shift();
        if (ackResolver) {
          clearTimeout(ackResolver.timer);
          ackResolver.reject(new Error('Received NACK from device.'));
        }
      } else {
        if (replyPromise.resolve) {
          replyPromise.resolve(message);
          replyPromise = {};
        }
      }
    }
  }
});

function waitForAck(timeout = 2000) {
  let timer;
  const promise = new Promise((resolve, reject) => {
    const resolver = {
      resolve: resolve,
      reject: reject,
      timer: null // Placeholder
    };
    timer = setTimeout(() => {
      // On timeout, find and remove the resolver from the queue
      const index = ackQueue.findIndex(p => p.timer === timer);
      if (index > -1) {
        ackQueue.splice(index, 1);
      }
      reject(new Error(`Timeout waiting for ACK after ${timeout}ms.`));
    }, timeout);
    resolver.timer = timer;
    ackQueue.push(resolver);
  });
  return promise;
}

function waitForReply(timeout = 2000) {
  let timer;
  return new Promise((resolve, reject) => {
    replyPromise = {
      resolve: (msg) => { clearTimeout(timer); resolve(msg); },
      reject: (err) => { clearTimeout(timer); reject(err); },
    };
    timer = setTimeout(() => {
      if (replyPromise.reject) {
        replyPromise.reject(new Error(`Timeout waiting for reply after ${timeout}ms.`));
        replyPromise = {};
      }
    }, timeout);
  });
}

async function sendMessage(payload) {
  const message = [0xF0, ...SYSEX_MANUFACTURER_ID, SYSEX_DEVICE_ID, ...payload, 0xF7];
  activeMidiOutput.sendMessage(message);
}

async function sendCommandAndWait(tag, body = []) {
  const payload = [tag, ...body];
  sendMessage(payload);
  await waitForAck();
}

async function begin_file_transfer(source_path, file_name) {
  console.log(`Copying ${source_path} to ${file_name}`);
  const encoded = new TextEncoder().encode(file_name);
  const encoded_with_null = new Uint8Array(encoded.length + 1);
  encoded_with_null.set(encoded);
  encoded_with_null[encoded.length] = 0;

  let bytes = [];
  for (let i = 0; i < encoded_with_null.length; i += 2) {
    const lower = encoded_with_null[i];
    const upper = (i + 1 < encoded_with_null.length) ? encoded_with_null[i + 1] : 0;
    bytes = bytes.concat(pack3_16((upper << 8) | lower));
  }

  await sendCommandAndWait(BEGIN_FILE_WRITE, bytes);
}

async function end_file_transfer() {
  await sendCommandAndWait(END_FILE_TRANSFER);
}

async function format_filesystem() {
  console.log("Sending command to format filesystem...");
  await sendCommandAndWait(FORMAT_FILESYSTEM);
}

async function get_firmware_version() {
  const tag = REQUEST_FIRMWARE_VERSION;
  const payload = [tag];
  sendMessage(payload);
  try {
    const reply = await waitForReply();
    if (reply[5] === REQUEST_FIRMWARE_VERSION) {
      const major = reply[6];
      const minor = reply[7];
      const patch = reply[8];
      console.log(`Device firmware version: v${major}.${minor}.${patch}`);
    } else {
      throw new Error(`Unexpected reply received. Tag: ${reply[5]}`);
    }
  } catch (e) {
    console.error(`Error requesting firmware version: ${e.message}`);
    throw e;
  }
}

function pack3_16(value) {
  return [(value >> 14) & 0x7F, (value >> 7) & 0x7F, value & 0x7F];
}

function encode_7_to_8(buffer) {
  let encoded = [];
  for (let i = 0; i < buffer.length; i += 7) {
    const chunk = buffer.slice(i, i + 7);
    let msbs = 0;
    let data_bytes = [];
    for (let j = 0; j < chunk.length; j++) {
      if (chunk[j] & 0x80) {
        msbs |= (1 << j);
      }
      data_bytes.push(chunk[j] & 0x7F);
    }
    encoded.push(...data_bytes);
    // Pad with zeros if chunk is smaller than 7
    for (let j = chunk.length; j < 7; j++) {
      encoded.push(0);
    }
    encoded.push(msbs);
  }
  return encoded;
}

async function send_file_content(data) {
  console.log("File data length: ", data.length);
  const total_bytes = data.length;
  const CHUNK_SIZE = 1022; // 146 * 7 bytes of raw data -> 146 * 8 = 1168 bytes of encoded data
  const PIPELINE_WINDOW = 8;
  let promises = [];

  for (let i = 0; i < total_bytes; i += CHUNK_SIZE) {
    const chunk = data.slice(i, i + CHUNK_SIZE);
    const encoded_chunk = encode_7_to_8(chunk);
    const payload = [FILE_BYTES, ...encoded_chunk];
    
    sendMessage(payload);
    promises.push(waitForAck());

    if (promises.length >= PIPELINE_WINDOW) {
      const results = await Promise.allSettled(promises);
      promises = [];
      const firstRejection = results.find(r => r.status === 'rejected');
      if (firstRejection) {
        throw new Error(`A transfer chunk failed. Reason: ${firstRejection.reason.message}`);
      }
    }

    const percentage = (i + chunk.length) / total_bytes;
    const bar_length = 40;
    const filled_length = Math.round(bar_length * percentage);
    const bar = '█'.repeat(filled_length) + '░'.repeat(bar_length - filled_length);
    
    process.stdout.clearLine(0);
    process.stdout.cursorTo(0);
    process.stdout.write(`Sending: ${Math.round(percentage * 100)}% |${bar}|`);
  }

  const finalResults = await Promise.allSettled(promises); // Wait for remaining ACKs
  const firstRejection = finalResults.find(r => r.status === 'rejected');
  if (firstRejection) {
    throw new Error(`A transfer chunk failed during final flush. Reason: ${firstRejection.reason.message}`);
  }

  process.stdout.clearLine(0);
  process.stdout.cursorTo(0);
  const bar_length = 40;
  const bar = '█'.repeat(bar_length);
  process.stdout.write(`Sending: 100% |${bar}| \n`);
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

process.on('SIGINT', () => {
  if (transferInProgress) {
    console.log('\n\nInterrupted during file transfer. Sending end command to device...');
    // Directly send the end transfer command without waiting for an ACK.
    sendMessage([END_FILE_TRANSFER]);
  }
  // The finally block in main will not run, so we must clean up here.
  console.log('Closing MIDI ports and exiting.');
  activeMidiOutput.closePort();
  activeMidiInput.closePort();
  process.exit(0);
});

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

      transferInProgress = true;
      const startTime = Date.now();
      await begin_file_transfer(source_path, sample_filename);
      const data = fs.readFileSync(source_path);
      await send_file_content(data);
      await end_file_transfer();
      transferInProgress = false;
      const duration = Date.now() - startTime;
      console.log(`Successfully transferred file in ${duration}ms.`);
    } else {
      console.log(`Error: Unknown command '${command}'. Use 'send' or 'format'.`);
      process.exit(1);
    }
  } catch (e) {
    console.error(`

Error: ${e.message}`);
    if (transferInProgress) {
      console.log('Aborting transfer on device...');
      // Fire-and-forget end transfer command. Don't wait for ACK.
      sendMessage([END_FILE_TRANSFER]);
    }
    process.exitCode = 1;
  } finally {
    activeMidiOutput.closePort();
    activeMidiInput.closePort();
  }
  console.log();
}

main();
