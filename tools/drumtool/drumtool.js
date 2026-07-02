#!/usr/bin/env node
/**
 * Drumtool - Comprehensive tool for DRUM device management
 * 
 * This tool implements MIDI Sample Dump Standard (SDS) for reliable
 * sample transfer and provides device management commands.
 * 
 * Features:
 * - On-the-fly WAV to PCM conversion (mono, 16-bit)
 * - SDS sample transfer with auto-slot assignment
 * - Batch sample uploads
 * - Firmware version checking
 * - Filesystem formatting
 * - Bootloader reboot
 * - ACK/NAK handshaking with timeout fallback
 * - Checksum validation and progress monitoring
 */

const midi = require('midi');
const fs = require('fs');
const readline = require('readline');
const { WaveFile } = require('wavefile');

// MIDI SDS Constants
const SYSEX_START = 0xF0;
const SYSEX_END = 0xF7;
const MIDI_NON_REALTIME_ID = 0x7E;

// SDS Message Types
const SDS_DUMP_HEADER = 0x01;
const SDS_DATA_PACKET = 0x02;
const SDS_DUMP_REQUEST = 0x03;
const SDS_ACK = 0x7F;
const SDS_NAK = 0x7E;
const SDS_CANCEL = 0x7D;
const SDS_WAIT = 0x7C;

// Device Configuration
const SYSEX_CHANNEL = 0x65; // DRUM device channel (same as existing protocol)
const PACKET_TIMEOUT = 20;   // 20ms timeout per packet as per SDS spec
const HEADER_TIMEOUT = 2000; // 2 second timeout for dump header

// Custom SysEx Configuration (for firmware/format commands)
const SYSEX_MANUFACTURER_ID = [0x00, 0x22, 0x01]; // Dato Musical Instruments
const SYSEX_DEVICE_ID = 0x65;                     // DRUM device ID

// Custom Protocol Commands
const REQUEST_FIRMWARE_VERSION = 0x01;
const REQUEST_STORAGE_INFO = 0x03;
const STORAGE_INFO_RESPONSE = 0x04;
const REBOOT_BOOTLOADER = 0x0B;
const FORMAT_FILESYSTEM = 0x15;
const CUSTOM_ACK = 0x13;
const CUSTOM_NACK = 0x14;

// Sequencer State Commands
const REQUEST_SEQUENCER_STATE = 0x30;
const SEQUENCER_STATE_RESPONSE = 0x31;
const SET_SEQUENCER_STATE = 0x32;
const SEQUENCER_NUM_TRACKS = 4;
const SEQUENCER_NUM_STEPS = 8;

// Settings Commands (generic key-value; ids match drum/settings.h)
const GET_SETTING = 0x40;
const SETTING_VALUE = 0x41;
const SET_SETTING = 0x42;
const SETTINGS = {
  midi_channel: { id: 0x01, min: 1, max: 16, description: 'MIDI channel for notes and CCs (1-16)' },
  slider_mode: { id: 0x02, min: 0, max: 7, description: 'Track slider bit mask: 1=pitch, 2=gain, 4=decay (combinable)' },
};

// Firmware Update Commands (UF2 streamed to the inactive A/B partition)
const BEGIN_FIRMWARE_UPDATE = 0x20;
const FIRMWARE_BYTES = 0x21;
const END_FIRMWARE_UPDATE = 0x22;
const ABORT_FIRMWARE_UPDATE = 0x23;
// Decoded bytes per FirmwareBytes message; must be a multiple of 7 so each
// message contains whole 8-byte encoded groups.
const FIRMWARE_CHUNK_SIZE = 7 * 146; // 1022 bytes
const FIRMWARE_ACK_TIMEOUT = 5000;

// Find and connect to DRUM device
function find_dato_drum() {
  const output = new midi.Output();
  const input = new midi.Input();
  const outPortCount = output.getPortCount();
  const inPortCount = input.getPortCount();
  const validPortNames = ["Pico", "DRUM"];

  let outPort = -1;
  let inPort = -1;
  let portName = "";

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
    }
    return null;
  }

  for (let i = 0; i < inPortCount; i++) {
    if (input.getPortName(i).includes(portName)) {
      inPort = i;
      break;
    }
  }

  if (inPort === -1) {
    console.error(`Found MIDI output "${portName}", but no matching input port.`);
    return null;
  }

  try {
    output.openPort(outPort);
    input.openPort(inPort);
    input.ignoreTypes(false, false, false);
    console.error(`Opened MIDI ports: ${portName}`);
    return { output, input };
  } catch (e) {
    console.error(`Error opening MIDI ports for ${portName}: ${e.message}`);
    return null;
  }
}

// MIDI ports will be initialized after argument validation
let activeMidiOutput = null;
let activeMidiInput = null;

// Message handling
let pendingResponse = null;
let deviceStatus = 'unknown'; // 'ok', 'slow', 'error', 'unknown'
let isTransferActive = false;
let currentTransferInfo = null; // To hold info about the active transfer

// Custom protocol handling
let customAckQueue = [];
let customReplyPromise = {};

function isSdsMessage(message) {
  return message[0] === SYSEX_START && 
         message[message.length - 1] === SYSEX_END &&
         message.length >= 6 &&
         message[1] === MIDI_NON_REALTIME_ID && 
         message[2] === SYSEX_CHANNEL;
}

function isCustomMessage(message) {
  return message[0] === SYSEX_START && 
         message[message.length - 1] === SYSEX_END &&
         message.length >= 6 &&
         message[1] === SYSEX_MANUFACTURER_ID[0] &&
         message[2] === SYSEX_MANUFACTURER_ID[1] &&
         message[3] === SYSEX_MANUFACTURER_ID[2] &&
         message[4] === SYSEX_DEVICE_ID;
}

// MIDI message handler - will be attached during initialization
function handleMidiMessage(deltaTime, message) {
  // Handle SDS messages
  if (isSdsMessage(message)) {
    const messageType = message[3];
    const packetNum = message.length > 4 ? message[4] : 0;
    
    // Handle responses during handshaking
    if (pendingResponse && (messageType === SDS_ACK || messageType === SDS_NAK || 
                           messageType === SDS_WAIT || messageType === SDS_CANCEL)) {
      clearTimeout(pendingResponse.timer);
      pendingResponse.resolve({ type: messageType, packet: packetNum });
      pendingResponse = null;
      return;
    }
    
    // Monitor device status even during non-handshaking mode
    if (isTransferActive) {
      if (messageType === SDS_WAIT) {
        deviceStatus = 'slow';
        if (process.stdout.clearLine) {
          process.stdout.write('\nDevice is busy processing...');
        } else {
          console.log('Device is busy processing...');
        }
      } else if (messageType === SDS_CANCEL) {
        deviceStatus = 'error';
        console.log('\nDevice cancelled transfer - storage may be full');
        // Could abort transfer here in future enhancement
      } else if (messageType === SDS_NAK) {
        console.log(`\nDevice rejected packet ${packetNum}`);
      } else if (messageType === SDS_ACK) {
        deviceStatus = 'ok';
      }
    }
  }
  
  // Handle custom protocol messages (firmware version, format, etc.)
  if (isCustomMessage(message)) {
    if (message.length < 6) return;
    const tag = message[5];

    if (tag === CUSTOM_ACK) {
      const ackResolver = customAckQueue.shift();
      if (ackResolver) {
        clearTimeout(ackResolver.timer);
        ackResolver.resolve();
      }
    } else if (tag === CUSTOM_NACK) {
      const ackResolver = customAckQueue.shift();
      if (ackResolver) {
        clearTimeout(ackResolver.timer);
        ackResolver.reject(new Error('Received NACK from device.'));
      }
    } else {
      if (customReplyPromise.resolve) {
        customReplyPromise.resolve(message);
        customReplyPromise = {};
      }
    }
  }
}

// Send universal SysEx message without SDS wrapping
function sendSysExMessage(data) {
  const message = [SYSEX_START, ...data, SYSEX_END];
  activeMidiOutput.sendMessage(message);
}

// Send SysEx message (for SDS protocol)
function sendSDSMessage(data) {
  const message = [SYSEX_START, MIDI_NON_REALTIME_ID, SYSEX_CHANNEL, ...data, SYSEX_END];
  activeMidiOutput.sendMessage(message);
}

// Send custom protocol message
function sendCustomMessage(payload) {
  const message = [SYSEX_START, ...SYSEX_MANUFACTURER_ID, SYSEX_DEVICE_ID, ...payload, SYSEX_END];
  activeMidiOutput.sendMessage(message);
}

// Wait for custom protocol ACK
function waitForCustomAck(timeout = 2000) {
  let timer;
  const promise = new Promise((resolve, reject) => {
    const resolver = {
      resolve: resolve,
      reject: reject,
      timer: null // Placeholder
    };
    timer = setTimeout(() => {
      // On timeout, find and remove the resolver from the queue
      const index = customAckQueue.findIndex(p => p.timer === timer);
      if (index > -1) {
        customAckQueue.splice(index, 1);
      }
      reject(new Error(`Timeout waiting for ACK after ${timeout}ms.`));
    }, timeout);
    resolver.timer = timer;
    customAckQueue.push(resolver);
  });
  return promise;
}

// Wait for custom protocol reply
function waitForCustomReply(timeout = 2000) {
  let timer;
  return new Promise((resolve, reject) => {
    customReplyPromise = {
      resolve: (msg) => { clearTimeout(timer); resolve(msg); },
      reject: (err) => { clearTimeout(timer); reject(err); },
    };
    timer = setTimeout(() => {
      if (customReplyPromise.reject) {
        customReplyPromise.reject(new Error(`Timeout waiting for reply after ${timeout}ms.`));
        customReplyPromise = {};
      }
    }, timeout);
  });
}

// Send custom command and wait for ACK
async function sendCustomCommandAndWait(tag, body = [], timeout = 2000) {
  const payload = [tag, ...body];
  sendCustomMessage(payload);
  await waitForCustomAck(timeout);
}

// Confirmation prompt for destructive operations
function askConfirmation(message) {
  return new Promise((resolve) => {
    const rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout
    });
    
    rl.question(`${message} (y/N): `, (answer) => {
      rl.close();
      const confirmed = answer.toLowerCase() === 'y' || answer.toLowerCase() === 'yes';
      resolve(confirmed);
    });
  });
}

// Get firmware version
async function get_firmware_version() {
  const tag = REQUEST_FIRMWARE_VERSION;
  const payload = [tag];
  sendCustomMessage(payload);
  try {
    const reply = await waitForCustomReply();
    if (reply[5] === REQUEST_FIRMWARE_VERSION) {
      const major = reply[6];
      const minor = reply[7];
      const patch = reply[8];
      console.log(`Device firmware version: v${major}.${minor}.${patch}`);
      return { major, minor, patch };
    } else {
      throw new Error(`Unexpected reply received. Tag: ${reply[5]}`);
    }
  } catch (e) {
    console.error(`Error requesting firmware version: ${e.message}`);
    throw e;
  }
}

// Get storage info
async function get_storage_info() {
  const tag = REQUEST_STORAGE_INFO;
  const payload = [tag];
  sendCustomMessage(payload);
  try {
    const reply = await waitForCustomReply();
    if (reply[5] === STORAGE_INFO_RESPONSE) {
      // Bounds check: ensure reply has expected length for storage info
      if (reply.length < 14) {
        throw new Error(`Insufficient reply data: expected at least 14 bytes, got ${reply.length}`);
      }
      // Parse storage info from reply - firmware uses 7-bit encoding across 4 bytes
      const total_bytes = ((reply[6] & 0x7F) << 21) | ((reply[7] & 0x7F) << 14) | ((reply[8] & 0x7F) << 7) | (reply[9] & 0x7F);
      const free_bytes = ((reply[10] & 0x7F) << 21) | ((reply[11] & 0x7F) << 14) | ((reply[12] & 0x7F) << 7) | (reply[13] & 0x7F);
      const used_bytes = total_bytes - free_bytes;

      console.log(`Device storage info:`);
      console.log(`  Total: ${formatBytes(total_bytes)}`);
      console.log(`  Used:  ${formatBytes(used_bytes)} (${Math.round(used_bytes / total_bytes * 100)}%)`);
      console.log(`  Free:  ${formatBytes(free_bytes)} (${Math.round(free_bytes / total_bytes * 100)}%)`);

      return { total_bytes, used_bytes, free_bytes };
    } else {
      throw new Error(`Unexpected reply received. Tag: ${reply[5]}`);
    }
  } catch (e) {
    console.error(`Error requesting storage info: ${e.message}`);
    throw e;
  }
}

// Get sequencer state
// Payload: 32 velocity bytes (track-major, 4 tracks x 8 steps) followed by
// 4 active-note bytes, all raw 7-bit values.
async function get_sequencer_state(asJson = false) {
  sendCustomMessage([REQUEST_SEQUENCER_STATE]);
  const reply = await waitForCustomReply();
  if (reply[5] !== SEQUENCER_STATE_RESPONSE) {
    throw new Error(`Unexpected reply received. Tag: ${reply[5]}`);
  }

  const payloadSize = SEQUENCER_NUM_TRACKS * SEQUENCER_NUM_STEPS + SEQUENCER_NUM_TRACKS;
  const payload = reply.slice(6, reply.length - 1);
  if (payload.length < payloadSize) {
    throw new Error(`Insufficient reply data: expected ${payloadSize} payload bytes, got ${payload.length}`);
  }

  const velocities = [];
  for (let track = 0; track < SEQUENCER_NUM_TRACKS; track++) {
    velocities.push(payload.slice(track * SEQUENCER_NUM_STEPS, (track + 1) * SEQUENCER_NUM_STEPS));
  }
  const activeNotes = payload.slice(
    SEQUENCER_NUM_TRACKS * SEQUENCER_NUM_STEPS,
    SEQUENCER_NUM_TRACKS * SEQUENCER_NUM_STEPS + SEQUENCER_NUM_TRACKS
  );

  const state = { velocities, activeNotes };

  if (asJson) {
    console.log(JSON.stringify(state, null, 2));
  } else {
    console.log('Sequencer state:');
    for (let track = 0; track < SEQUENCER_NUM_TRACKS; track++) {
      const steps = velocities[track].map(v => v.toString().padStart(3)).join(' ');
      console.log(`  Track ${track} (note ${activeNotes[track]}): ${steps}`);
    }
  }

  return state;
}

// Look up a setting by name, with a helpful error listing known names
function findSetting(name) {
  const setting = SETTINGS[name];
  if (!setting) {
    throw new Error(`Unknown setting '${name}'. Known settings: ${Object.keys(SETTINGS).join(', ')}`);
  }
  return setting;
}

// Get one named setting, or all settings when name is undefined
async function get_setting(name) {
  const names = name ? [name] : Object.keys(SETTINGS);
  for (const settingName of names) {
    const setting = findSetting(settingName);
    sendCustomMessage([GET_SETTING, setting.id]);
    const reply = await waitForCustomReply();
    if (reply[5] !== SETTING_VALUE || reply[6] !== setting.id) {
      throw new Error(`Unexpected reply for '${settingName}'. Tag: ${reply[5]}`);
    }
    console.log(`${settingName}: ${reply[7]}`);
  }
}

// Set one named setting; the device validates and persists the value
async function set_setting(name, valueStr) {
  const setting = findSetting(name);
  const value = parseInt(valueStr, 10);
  if (isNaN(value) || value < setting.min || value > setting.max) {
    throw new Error(`Invalid value '${valueStr}' for '${name}'. Must be ${setting.min}-${setting.max}.`);
  }
  await sendCustomCommandAndWait(SET_SETTING, [setting.id, value]);
  console.log(`${name} set to ${value}.`);
}

// Validate and normalize a sequencer state object ({ velocities, activeNotes })
function validateSequencerState(state) {
  if (!state || !Array.isArray(state.velocities) || !Array.isArray(state.activeNotes)) {
    throw new Error("State must be an object with 'velocities' and 'activeNotes' arrays.");
  }
  if (state.velocities.length !== SEQUENCER_NUM_TRACKS) {
    throw new Error(`Expected ${SEQUENCER_NUM_TRACKS} velocity tracks, got ${state.velocities.length}`);
  }
  if (state.activeNotes.length !== SEQUENCER_NUM_TRACKS) {
    throw new Error(`Expected ${SEQUENCER_NUM_TRACKS} active notes, got ${state.activeNotes.length}`);
  }
  const check7bit = (value, what) => {
    if (!Number.isInteger(value) || value < 0 || value > 127) {
      throw new Error(`${what} must be an integer 0-127, got ${value}`);
    }
  };
  state.velocities.forEach((track, t) => {
    if (!Array.isArray(track) || track.length !== SEQUENCER_NUM_STEPS) {
      throw new Error(`Track ${t} must have ${SEQUENCER_NUM_STEPS} velocity values.`);
    }
    track.forEach((v, s) => check7bit(v, `Velocity for track ${t} step ${s}`));
  });
  state.activeNotes.forEach((note, t) => check7bit(note, `Active note for track ${t}`));
}

// Set sequencer state from a JSON file (or stdin with '-')
async function set_sequencer_state(filePath) {
  const source = filePath === '-' ? '/dev/stdin' : filePath;
  let state;
  try {
    state = JSON.parse(fs.readFileSync(source, 'utf8'));
  } catch (e) {
    throw new Error(`Could not read state from '${filePath}': ${e.message}`);
  }
  validateSequencerState(state);

  const payload = [];
  for (let track = 0; track < SEQUENCER_NUM_TRACKS; track++) {
    payload.push(...state.velocities[track]);
  }
  payload.push(...state.activeNotes);

  await sendCustomCommandAndWait(SET_SEQUENCER_STATE, payload);
  console.log('Sequencer state set successfully.');
}

// Format bytes for human-readable display
function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

// Format filesystem
async function format_filesystem() {
  console.log("⚠️  WARNING: This will erase ALL files on the device filesystem!");
  const confirmed = await askConfirmation("Are you sure you want to format the filesystem?");

  if (!confirmed) {
    console.log("Format cancelled.");
    return;
  }

  console.log("Sending command to format filesystem...");
  console.log("This may take up to 30 seconds depending on partition size...");
  await sendCustomCommandAndWait(FORMAT_FILESYSTEM, [], 30000);
  console.log("Successfully formatted filesystem.");
}

// Reboot to bootloader
async function reboot_bootloader() {
  console.log("⚠️  WARNING: This will reboot the device into bootloader mode!");
  console.log("The device will disconnect and enter firmware update mode.");
  const confirmed = await askConfirmation("Are you sure you want to reboot to bootloader?");
  
  if (!confirmed) {
    console.log("Reboot cancelled.");
    return;
  }
  
  console.log("Sending command to reboot to bootloader...");
  const payload = [REBOOT_BOOTLOADER];
  sendCustomMessage(payload);
  // Note: Device will reboot immediately, so we don't wait for ACK
  console.log("Reboot command sent. Device should now enter bootloader mode.");
}

// --- Firmware update over SysEx ---

// Packs 8-bit data into SysEx-safe 8-byte groups: 7 data bytes (low 7 bits)
// followed by one byte carrying the MSBs. Mirrors codec::decode_8_to_7 in the
// firmware. The final group is zero-padded; the device ignores the padding.
function encode8to7(data) {
  const out = [];
  for (let i = 0; i < data.length; i += 7) {
    let msbs = 0;
    for (let j = 0; j < 7; j++) {
      const byte = i + j < data.length ? data[i + j] : 0;
      out.push(byte & 0x7F);
      msbs |= ((byte >> 7) & 0x01) << j;
    }
    out.push(msbs);
  }
  return out;
}

// Encodes raw bytes as 16-bit values, 3 SysEx bytes per value (matches
// codec::decode_3_to_16bit in the firmware). Used for the Begin payload.
function encode3to16(bytes) {
  const out = [];
  for (let i = 0; i < bytes.length; i += 2) {
    const value = bytes[i] | ((i + 1 < bytes.length ? bytes[i + 1] : 0) << 8);
    out.push((value >> 14) & 0x7F, (value >> 7) & 0x7F, value & 0x7F);
  }
  return out;
}

const UF2_MAGIC_START_0 = 0x0A324655;
const UF2_BLOCK_SIZE = 512;

async function flash_firmware(filePath) {
  const crypto = require('crypto');
  const uf2 = fs.readFileSync(filePath);

  if (uf2.length === 0 || uf2.length % UF2_BLOCK_SIZE !== 0) {
    throw new Error(`${filePath} is not a UF2 file (size not a multiple of 512).`);
  }
  if (uf2.readUInt32LE(0) !== UF2_MAGIC_START_0) {
    throw new Error(`${filePath} is not a UF2 file (bad magic).`);
  }

  const sha256 = crypto.createHash('sha256').update(uf2).digest();
  console.log(`Firmware: ${filePath} (${uf2.length} bytes, ${uf2.length / UF2_BLOCK_SIZE} UF2 blocks)`);
  console.log(`SHA-256:  ${sha256.toString('hex')}`);

  try {
    await get_firmware_version();
  } catch (e) {
    console.log('Could not read current firmware version, continuing anyway.');
  }

  // Begin: total size (32-bit LE) + SHA-256 + version placeholder (the device
  // treats the version as informational).
  const beginPayload = [
    uf2.length & 0xFF, (uf2.length >> 8) & 0xFF,
    (uf2.length >> 16) & 0xFF, (uf2.length >> 24) & 0xFF,
    ...sha256, 0, 0, 0,
  ];
  sendCustomMessage([BEGIN_FIRMWARE_UPDATE, ...encode3to16(beginPayload)]);
  await waitForCustomAck(FIRMWARE_ACK_TIMEOUT);
  console.log('Device accepted firmware update, streaming...');

  const startTime = Date.now();
  try {
    for (let offset = 0; offset < uf2.length; offset += FIRMWARE_CHUNK_SIZE) {
      const chunk = uf2.subarray(offset, Math.min(offset + FIRMWARE_CHUNK_SIZE, uf2.length));
      sendCustomMessage([FIRMWARE_BYTES, ...encode8to7(chunk)]);
      await waitForCustomAck(FIRMWARE_ACK_TIMEOUT);

      const done = Math.min(offset + FIRMWARE_CHUNK_SIZE, uf2.length);
      const percent = Math.floor((done / uf2.length) * 100);
      const barLength = 30;
      const filled = Math.floor((done / uf2.length) * barLength);
      const bar = '█'.repeat(filled) + '░'.repeat(barLength - filled);
      process.stdout.write(`\r[${bar}] ${percent}% (${done}/${uf2.length} bytes)`);
    }
  } catch (e) {
    process.stdout.write('\n');
    sendCustomMessage([ABORT_FIRMWARE_UPDATE]);
    throw new Error(`Transfer failed: ${e.message}`);
  }
  process.stdout.write('\n');

  sendCustomMessage([END_FIRMWARE_UPDATE]);
  await waitForCustomAck(FIRMWARE_ACK_TIMEOUT);

  const elapsed = ((Date.now() - startTime) / 1000).toFixed(1);
  console.log(`Firmware verified by device in ${elapsed}s.`);
  console.log('Device is rebooting into the new firmware for a trial boot.');
  console.log('It commits the update automatically after a few seconds of healthy operation.');
  console.log("Run 'drumtool.js version' after the device reconnects to confirm.");
}

// Test universal SysEx identity request
async function test_universal_identity() {
  console.log("Testing universal SysEx identity request...");

  // Send universal identity request: F0 7E 7F 06 01 F7
  const identityRequest = [MIDI_NON_REALTIME_ID, 0x7F, 0x06, 0x01];

  console.log("Sending universal identity request: F0 7E 7F 06 01 F7");
  sendSysExMessage(identityRequest);

  // Listen for identity response
  let responseReceived = false;
  const originalHandler = activeMidiInput.listeners('message')[0];

  const identityHandler = (deltaTime, message) => {
    if (message[0] === SYSEX_START) {
      const sysexData = message.slice(1, -1); // Remove F0 and F7

      if (sysexData.length >= 5 &&
          sysexData[0] === 0x7E && sysexData[1] === 0x7F &&
          sysexData[2] === 0x06 && sysexData[3] === 0x02) {

        responseReceived = true;
        console.log("\n✅ Universal identity response received!");
        console.log("Raw message:", message.map(b => `0x${b.toString(16).padStart(2, '0')}`).join(' '));

        if (sysexData.length >= 8) {
          const manufacturerId = sysexData.slice(4, 7);
          console.log("Manufacturer ID:", manufacturerId.map(b => `0x${b.toString(16).padStart(2, '0')}`).join(' '));

          if (sysexData.length >= 10) {
            const deviceFamily = (sysexData[8] | (sysexData[9] << 8));
            console.log("Device Family:", `0x${deviceFamily.toString(16).padStart(4, '0')}`);
          }

          if (sysexData.length >= 12) {
            const deviceMember = (sysexData[10] | (sysexData[11] << 8));
            console.log("Device Member:", `0x${deviceMember.toString(16).padStart(4, '0')}`);
          }

          if (sysexData.length >= 15) {
            const softwareRev = sysexData.slice(12, sysexData.length);
            console.log("Software Revision:", softwareRev.map(v => v.toString()).join('.'));
          }
        }
      }
    }

    // Also call original handler for other messages
    if (originalHandler) {
      originalHandler(deltaTime, message);
    }
  };

  // Replace message handler temporarily
  activeMidiInput.removeAllListeners('message');
  activeMidiInput.on('message', identityHandler);

  // Wait for response
  await new Promise(resolve => setTimeout(resolve, 1000));

  // Restore original handler
  activeMidiInput.removeAllListeners('message');
  if (originalHandler) {
    activeMidiInput.on('message', originalHandler);
  }

  if (!responseReceived) {
    console.log("❌ No universal identity response received within timeout");
    console.log("This may indicate the device doesn't support universal identity requests");
  }
}

// Wait for ACK/NAK/WAIT/CANCEL with timeout and CTRL+C escape
function waitForResponse(timeout, packetNum = 0, allowWaitEscape = true) {
  return new Promise((resolve) => {
    // For WAIT responses, use longer timeout but still have escape hatch
    const maxWaitTime = allowWaitEscape ? 30000 : timeout; // 30s max even for WAIT
    const timer = setTimeout(() => {
      pendingResponse = null;
      resolve({ type: 'timeout', packet: packetNum });
    }, maxWaitTime);

    pendingResponse = {
      resolve,
      timer
    };
  });
}

// Convert sample rate to SDS period format (3-byte nanosecond period)
function sampleRateToPeriod(sampleRate) {
  const periodNs = Math.round(1000000000 / sampleRate);
  return [
    periodNs & 0x7F,           // bits 0-6
    (periodNs >> 7) & 0x7F,    // bits 7-13
    (periodNs >> 14) & 0x7F    // bits 14-20
  ];
}

// Convert length to SDS 3-byte format (word count, where word = 2 bytes)
function lengthToSdsFormat(byteLength) {
  const wordLength = Math.floor(byteLength / 2); // SDS uses word count
  return [
    wordLength & 0x7F,         // bits 0-6
    (wordLength >> 7) & 0x7F,  // bits 7-13
    (wordLength >> 14) & 0x7F  // bits 14-20
  ];
}

// Create SDS Dump Header
function createDumpHeader(sampleNum, bitDepth, sampleRate, sampleLength) {
  const sampleNumBytes = [sampleNum & 0x7F, (sampleNum >> 7) & 0x7F];
  const periodBytes = sampleRateToPeriod(sampleRate);
  const lengthBytes = lengthToSdsFormat(sampleLength);
  
  // Loop points set to no-loop (start and end = length, type = 0x7F)
  const loopStartBytes = lengthBytes.slice(); // Copy length
  const loopEndBytes = lengthBytes.slice();   // Copy length
  const loopType = 0x7F; // No loop

  return [
    SDS_DUMP_HEADER,
    ...sampleNumBytes,    // sl sh - sample number
    bitDepth,             // ee - bits per sample
    ...periodBytes,       // pl pm ph - sample period in nanoseconds
    ...lengthBytes,       // gl gm gh - length in words
    ...loopStartBytes,    // hl hm hh - loop start (set to length for no-loop)
    ...loopEndBytes,      // il im ih - loop end (set to length for no-loop)
    loopType              // jj - loop type (0x7F = no loop)
  ];
}

// Pack 16-bit sample into SDS 3-byte format
function pack16BitSample(sample) {
  // Convert signed to unsigned (SDS uses 0x0000 as full negative)
  const unsignedSample = sample + 0x8000;
  
  // Pack into 3 bytes (left-justified)
  return [
    (unsignedSample >> 9) & 0x7F,  // bits 15-9
    (unsignedSample >> 2) & 0x7F,  // bits 8-2
    (unsignedSample << 5) & 0x7F   // bits 1-0 (left-shifted to bits 6-5)
  ];
}

// Calculate XOR checksum for data packet
function calculateChecksum(packetNum, dataBytes) {
  let checksum = MIDI_NON_REALTIME_ID ^ SYSEX_CHANNEL ^ SDS_DATA_PACKET ^ packetNum;
  for (const byte of dataBytes) {
    checksum ^= byte;
  }
  return checksum & 0x7F; // Clear high bit
}

// Create SDS Data Packet
function createDataPacket(packetNum, pcmData, offset) {
  const dataBytes = [];
  
  // Pack up to 40 16-bit samples (120 bytes)
  for (let i = 0; i < 40; i++) {
    const sampleOffset = offset + (i * 2);
    if (sampleOffset + 1 < pcmData.length) {
      // Read 16-bit little-endian sample
      const sample = pcmData.readInt16LE(sampleOffset);
      dataBytes.push(...pack16BitSample(sample));
    } else {
      // Pad with zeros if we're at the end
      dataBytes.push(0, 0, 0);
    }
  }
  
  // Ensure exactly 120 bytes
  while (dataBytes.length < 120) {
    dataBytes.push(0);
  }
  dataBytes.length = 120;
  
  const checksum = calculateChecksum(packetNum, dataBytes);
  
  return [
    SDS_DATA_PACKET,
    packetNum & 0x7F,
    ...dataBytes,
    checksum
  ];
}

// Batch sample transfer function
async function transferMultipleSamples(transfers, verboseMode = false) {
  console.log(`\n=== Batch SDS Transfer: ${transfers.length} samples ===`);
  
  const results = [];
  let successCount = 0;
  
  for (let i = 0; i < transfers.length; i++) {
    const { filePath, slot, sampleRate } = transfers[i];
    console.log(`\n[${i + 1}/${transfers.length}] Transferring ${filePath} → slot ${slot}`);
    
    try {
      const success = await transferSample(filePath, slot, sampleRate, verboseMode);
      results.push({ filePath, slot, success, error: null });
      if (success) {
        successCount++;
      }
    } catch (error) {
      console.error(`Failed: ${error.message}`);
      results.push({ filePath, slot, success: false, error: error.message });
    }
    
    // Brief pause between transfers to avoid overwhelming the device
    if (i < transfers.length - 1) {
      await new Promise(resolve => setTimeout(resolve, 100));
    }
  }
  
  // Summary
  console.log(`\n=== Transfer Summary ===`);
  console.log(`Successful: ${successCount}/${transfers.length}`);
  
  if (successCount < transfers.length) {
    console.log('\nFailed transfers:');
    results.filter(r => !r.success).forEach(r => {
      console.log(`  ${r.filePath} → slot ${r.slot}: ${r.error}`);
    });
  }
  
  return successCount === transfers.length;
}

// Main sample transfer function
async function transferSample(filePath, sampleNumber, sampleRate = 44100, verboseMode = false) {
  const verbose = verboseMode;

  // Validate sample number
  if (sampleNumber < 0 || sampleNumber > 127) {
    console.error(`Error: Sample number must be between 0-127, got: ${sampleNumber}`);
    return false;
  }

  const targetFilename = `${sampleNumber.toString().padStart(2, '0')}.pcm`;
  console.log(`\n=== SDS Transfer: ${filePath} → /${targetFilename} (slot ${sampleNumber}) ===`);

  if (!fs.existsSync(filePath)) {
    console.error(`Error: Source file not found at '${filePath}'`);
    return false;
  }

  let pcmData;
  let finalSampleRate;

  try {
    const fileBuffer = fs.readFileSync(filePath);
    // Try to parse as WAV file
    const wav = new WaveFile(fileBuffer);

    // If successful, convert to device-compatible format (16-bit, mono)
    if (wav.fmt.bitsPerSample !== 16) {
      if (verbose) console.log(`Converting from ${wav.fmt.bitsPerSample}-bit to 16-bit...`);
      wav.toBitDepth('16');
    }

    finalSampleRate = wav.fmt.sampleRate;
    if (verbose && sampleRate !== 44100 && sampleRate !== finalSampleRate) {
      console.log(`Note: Using sample rate from WAV file (${finalSampleRate}Hz) instead of command-line argument.`);
    }

    // Get samples and convert to mono if needed
    let samples = wav.getSamples(false); // Get as array of samples
    if (wav.fmt.numChannels !== 1) {
      if (verbose) console.log('Converting stereo to mono...');
      // Convert stereo to mono by averaging left and right channels
      const monoSamples = [];
      for (let i = 0; i < samples.length; i += wav.fmt.numChannels) {
        let sum = 0;
        for (let ch = 0; ch < wav.fmt.numChannels; ch++) {
          sum += samples[i + ch] || 0;
        }
        monoSamples.push(sum / wav.fmt.numChannels);
      }
      samples = monoSamples;
    }

    // Convert samples array back to buffer (16-bit little endian)
    pcmData = Buffer.alloc(samples.length * 2);
    for (let i = 0; i < samples.length; i++) {
      const sample = Math.max(-32768, Math.min(32767, Math.round(samples[i])));
      pcmData.writeInt16LE(sample, i * 2);
    }

    console.log(`Processed WAV: ${pcmData.length} bytes, ${finalSampleRate}Hz, 16-bit mono`);

  } catch (e) {
    // If parsing fails, assume it's a raw PCM file
    console.log(`Could not parse as WAV file. Assuming raw 16-bit PCM data.`);
    if (verbose) console.log(`(Error: ${e.message})`);
    pcmData = fs.readFileSync(filePath);
    finalSampleRate = sampleRate; // Use the rate from command line
    console.log(`Using raw file data: ${pcmData.length} bytes, ${finalSampleRate}Hz`);
  }
  
  // Calculate transfer parameters
  const packetsNeeded = Math.ceil(pcmData.length / 80); // 40 samples * 2 bytes per packet
  if (verbose) {
    console.log(`Transfer will require ${packetsNeeded} data packets`);
  }
  
  try {
    isTransferActive = true;
    currentTransferInfo = { sampleNumber };
    deviceStatus = 'unknown';
    const transferStartTime = Date.now();
    
    // Step 1: Send Dump Header
    const header = createDumpHeader(sampleNumber, 16, finalSampleRate, pcmData.length);
    if (verbose) {
      console.log("\n1. Sending Dump Header...");
      console.log("Header bytes:", header.length, ">", header.map(b => `0x${b.toString(16).padStart(2, '0')}`).join(' '));
    }
    sendSDSMessage(header);
    
    const headerResponse = await waitForResponse(HEADER_TIMEOUT);
    if (headerResponse.type === SDS_NAK) {
      if (verbose) {
        console.log("Header NAK received, retrying...");
      }
      sendSDSMessage(createDumpHeader(sampleNumber, 16, finalSampleRate, pcmData.length));
      const retryResponse = await waitForResponse(HEADER_TIMEOUT);
      if (retryResponse.type !== SDS_ACK && retryResponse.type !== 'timeout') {
        throw new Error("Header rejected twice");
      }
    }
    
    let handshaking = headerResponse.type === SDS_ACK;
    if (verbose) {
      console.log(`Header ${headerResponse.type === SDS_ACK ? 'ACK' : headerResponse.type} - ${handshaking ? 'handshaking mode' : 'non-handshaking mode'}`);
      console.log("\n2. Sending data packets...");
    }
    let packetNum = 0;
    let offset = 0;
    let successfulPackets = 0;
    
    const totalPackets = Math.ceil(pcmData.length / 80);
    const showProgress = totalPackets > 4;
    let lastProgressUpdate = 0;
    
    while (offset < pcmData.length) {
      if (stallAfter !== null && packetNum >= stallAfter) {
        console.log(`\nTEST: stalling after ${packetNum} packets — exiting without SDS cancel.`);
        process.exit(0);
      }
      const packet = createDataPacket(packetNum & 0x7F, pcmData, offset);
      if (verbose && packetNum < 5) { // Debug first few packets
        console.log(`Packet ${packetNum} size:`, packet.length, "checksum:", `0x${packet[packet.length-1].toString(16)}`);
      }
      sendSDSMessage(packet);
      
      if (handshaking) {
        const response = await waitForResponse(PACKET_TIMEOUT, packetNum & 0x7F);
        
        if (response.type === SDS_ACK) {
          deviceStatus = 'ok';
          successfulPackets++;
          offset += 80; // Move to next 40 samples (80 bytes)
          packetNum++;
        } else if (response.type === SDS_NAK) {
          if (verbose) {
            if (showProgress) {
              console.log(`\nPacket ${packetNum & 0x7F} NAK - retrying`);
            } else {
              console.log(`Packet ${packetNum & 0x7F} NAK - retrying`);
            }
          }
          continue; // Retry same packet
        } else if (response.type === SDS_WAIT) {
          if (verbose) {
            console.log(`\nPacket ${packetNum & 0x7F} WAIT - device is busy, waiting for final response...`);
          }
          deviceStatus = 'slow';
          // Wait indefinitely for ACK/NAK/CANCEL after WAIT
          const finalResponse = await waitForResponse(30000, packetNum & 0x7F, true);
          if (finalResponse.type === SDS_ACK) {
            successfulPackets++;
            offset += 80;
            packetNum++;
          } else if (finalResponse.type === SDS_NAK) {
            continue; // Retry same packet
          } else if (finalResponse.type === SDS_CANCEL) {
            throw new Error('Device cancelled transfer - storage may be full');
          } else {
            // Even WAIT can timeout - assume non-handshaking
            if (verbose) {
              console.log(`Final response timeout after WAIT - assuming non-handshaking`);
            }
            successfulPackets++;
            offset += 80;
            packetNum++;
            handshaking = false;
          }
        } else if (response.type === SDS_CANCEL) {
          throw new Error('Device cancelled transfer - storage may be full');
        } else {
          // Timeout - assume non-handshaking mode
          if (verbose) {
            if (showProgress) {
              console.log(`\nPacket ${packetNum & 0x7F} timeout - continuing without handshaking`);
            } else {
              console.log(`Packet ${packetNum & 0x7F} timeout - continuing without handshaking`);
            }
          }
          successfulPackets++;
          offset += 80;
          packetNum++;
          // Switch to non-handshaking mode for remaining packets
          handshaking = false;
        }
      } else {
        // Non-handshaking mode - advance immediately
        successfulPackets++;
        offset += 80;
        packetNum++;
      }
      
      // Progress indicator (skip for very small transfers)
      if (showProgress) {
        const progress = Math.min(100, (successfulPackets / totalPackets) * 100);
        
        // Only update progress every 1% or every 50 packets to reduce flicker
        const progressPercent = Math.round(progress);
        if (progressPercent > lastProgressUpdate || successfulPackets % 50 === 0) {
          lastProgressUpdate = progressPercent;
          
          const bar_length = 40;
          const filled_length = Math.round(bar_length * (progress / 100));
          const bar = '█'.repeat(filled_length) + '░'.repeat(bar_length - filled_length);
          
          // Status indicator based on device feedback
          const statusText = deviceStatus === 'slow' ? '[BUSY]' : 
                            deviceStatus === 'error' ? '[ERROR]' : 
                            deviceStatus === 'ok' ? '[OK]' : '[??]';
          
          if (process.stdout.clearLine) {
            process.stdout.clearLine(0);
            process.stdout.cursorTo(0);
            process.stdout.write(`${statusText} Progress: ${progressPercent}% |${bar}| ${successfulPackets}/${totalPackets} packets`);
          } else {
            // Fallback for environments without clearLine
            console.log(`${statusText} Progress: ${progressPercent}% |${bar}| ${successfulPackets}/${totalPackets} packets`);
          }
        }
      }
    }
    
    const transferDuration = Date.now() - transferStartTime;
    const transferSeconds = (transferDuration / 1000).toFixed(1);
    console.log(`\n\nTransfer complete: ${successfulPackets} packets sent in ${transferSeconds} s`);
    return true;
    
  } catch (error) {
    console.error(`\nTransfer failed: ${error.message}`);
    return false;
  } finally {
    isTransferActive = false;
    currentTransferInfo = null;
    deviceStatus = 'unknown';
  }
}

// Helper function to check if argument is a sample rate
function isSampleRate(arg) {
  const rateMatch = arg.match(/^(\d+)$/);
  return rateMatch && !arg.includes(':') && parseInt(arg, 10) >= 1000; // Reasonable sample rate threshold
}

// Parse explicit file:slot format
function parseExplicitSlots(fileArgs, sampleRate) {
  const transfers = [];
  
  for (const arg of fileArgs) {
    const colonIndex = arg.indexOf(':');
    if (colonIndex === -1) {
      throw new Error(`Mixed formats not allowed. Use either 'filename:slot' for all files or just 'filename' for all files.`);
    }
    
    const filePath = arg.substring(0, colonIndex);
    const slotStr = arg.substring(colonIndex + 1);
    const slot = parseInt(slotStr, 10);
    
    if (isNaN(slot) || slot < 0 || slot > 127) {
      throw new Error(`Invalid slot number '${slotStr}'. Must be 0-127.`);
    }
    
    transfers.push({ filePath, slot, sampleRate });
  }
  
  return transfers;
}

// Parse auto-increment format (just filenames)
function parseAutoIncrement(fileArgs, sampleRate) {
  const transfers = [];
  const AUTO_SLOT_OFFSET = 30; // Start auto-assignment from slot 30

  fileArgs.forEach((filePath, index) => {
    const slot = index + AUTO_SLOT_OFFSET;
    if (slot > 127) {
      throw new Error(`Too many files. Maximum ${128 - AUTO_SLOT_OFFSET} files when auto-assigning from slot ${AUTO_SLOT_OFFSET}.`);
    }
    transfers.push({ filePath, slot, sampleRate });
  });

  return transfers;
}

// Parse file:slot arguments with auto-increment support
function parseFileSlotArgs(args) {
  let sampleRate = 44100;
  
  // Extract sample rate and filter file arguments
  const fileArgs = [];
  for (const arg of args) {
    if (arg === '--verbose' || arg === '-v') {
      continue; // Already handled globally
    }
    
    if (isSampleRate(arg)) {
      sampleRate = parseInt(arg, 10);
      continue;
    }
    
    fileArgs.push(arg);
  }
  
  if (fileArgs.length === 0) {
    return [];
  }
  
  // Detect mode: check if ANY argument contains ':'
  const hasSlotSyntax = fileArgs.some(arg => arg.includes(':'));
  
  if (hasSlotSyntax) {
    // Mode A: ALL must use filename:slot format
    return parseExplicitSlots(fileArgs, sampleRate);
  } else {
    // Mode B: ALL are filenames, auto-increment from 0
    return parseAutoIncrement(fileArgs, sampleRate);
  }
}

// Command line interface
const command = process.argv[2];
const verbose = process.argv.includes('--verbose') || process.argv.includes('-v');
// TEST ONLY: --stall-after=N abandons the transfer after N data packets
// without sending an SDS cancel, simulating a host that vanished mid-dump.
const stallArg = process.argv.find(arg => arg.startsWith('--stall-after='));
const stallAfter = stallArg ? parseInt(stallArg.split('=')[1], 10) : null;

if (!command) {
  console.log("Drumtool - DRUM Device Management Tool");
  console.log("");
  console.log("Usage:");
  console.log("  drumtool.js send <file:slot> [file:slot] ... [sample_rate] [--verbose|-v]");
  console.log("  drumtool.js version");
  console.log("  drumtool.js storage");
  console.log("  drumtool.js format");
  console.log("  drumtool.js reboot-bootloader");
  console.log("  drumtool.js identity");
  console.log("  drumtool.js flash <firmware.uf2>");
  console.log("  drumtool.js get-sequencer [--json]");
  console.log("  drumtool.js set-sequencer <state.json|->");
  console.log("  drumtool.js get-setting [name]");
  console.log("  drumtool.js set-setting <name> <value>");
  console.log("");
  console.log("Commands:");
  console.log("  send           - Transfer audio samples (WAV or raw PCM) using SDS protocol");
  console.log("  version        - Get device firmware version");
  console.log("  storage        - Get device storage information");
  console.log("  format         - Format device filesystem");
  console.log("  reboot-bootloader - Reboot device into bootloader mode");
  console.log("  identity       - Test universal SysEx identity request");
  console.log("  flash          - Update firmware over MIDI (A/B partition, auto-revert on failure)");
  console.log("  get-sequencer  - Read the sequencer pattern (velocities + active notes)");
  console.log("  set-sequencer  - Write a sequencer pattern from a JSON file ('-' for stdin)");
  console.log("  get-setting    - Read a device setting (all settings when no name given)");
  console.log("  set-setting    - Write a device setting (persisted on the device)");
  console.log("");
  console.log("Settings:");
  for (const [name, setting] of Object.entries(SETTINGS)) {
    console.log(`  ${name.padEnd(14)} - ${setting.description}`);
  }
  console.log("");
  console.log("Send Arguments:");
  console.log("  file:slot      - Audio file path and target slot (0-127)");
  console.log("  file           - Audio file path (auto-assigns slots 30,31,32... in order)");
  console.log("  sample_rate    - Sample rate in Hz for raw PCM files (default: 44100).");
  console.log("                   For WAV files, the sample rate is detected automatically.");
  console.log("  --verbose, -v  - Show detailed transfer information");
  console.log("");
  console.log("Examples:");
  console.log("  # Explicit slot assignment (WAV files are auto-converted):");
  console.log("  drumtool.js send kick.wav:0 snare.wav:1");
  console.log("  ");
  console.log("  # Auto-increment slots (starts from 30):");
  console.log("  drumtool.js send kick.wav snare.wav hat.wav   # Slots 30,31,32 automatically");
  console.log("  ");
  console.log("  # Transfer raw PCM data with a specific sample rate:");
  console.log("  drumtool.js send my_sample.pcm:10 22050 -v");
  console.log("  ");
  console.log("  # Cannot mix formats - use either all file:slot OR all filenames");
  console.log("  drumtool.js version                           # Check firmware version");
  console.log("  drumtool.js storage                           # Check storage usage");
  console.log("  drumtool.js format                            # Format filesystem");
  console.log("  drumtool.js reboot-bootloader                 # Enter bootloader mode");
  console.log("  drumtool.js get-sequencer --json > state.json # Save sequencer pattern");
  console.log("  drumtool.js set-sequencer state.json          # Restore sequencer pattern");
  console.log("  drumtool.js set-setting midi_channel 1        # Use MIDI channel 1");
  console.log("");
  process.exit(1);
}

// Enhanced cleanup handler with proper escape from all states
function cleanup_and_exit() {
  if (activeMidiOutput || activeMidiInput) {
    console.log('\n\nClosing MIDI ports and exiting...');
    if (activeMidiOutput) {
      activeMidiOutput.closePort();
    }
    if (activeMidiInput) {
      activeMidiInput.closePort();
    }
  }
  process.exit(0);
}

process.on('SIGINT', () => {
  console.log('\n\nStopping transfer...');
  if (isTransferActive && currentTransferInfo && activeMidiOutput) {
    const { sampleNumber } = currentTransferInfo;
    const sampleNumBytes = [sampleNumber & 0x7F, (sampleNumber >> 7) & 0x7F];
    const cancelMessage = [SDS_CANCEL, ...sampleNumBytes];
    sendSDSMessage(cancelMessage);
    console.log(`Cancel command sent for sample ${sampleNumber}.`);
  }

  if (pendingResponse) {
    clearTimeout(pendingResponse.timer);
    pendingResponse = null;
  }
  isTransferActive = false;

  // A short delay to allow the cancel message to be sent before closing ports
  setTimeout(cleanup_and_exit, 200);
});

process.on('SIGTERM', cleanup_and_exit);

// Main execution
async function main() {
  try {
    // Validate arguments early before MIDI initialization
    if (command === 'send') {
      const args = process.argv.slice(3).filter(arg => arg !== '--verbose' && arg !== '-v' && !arg.startsWith('--stall-after='));
      
      if (args.length === 0) {
        console.error("Error: 'send' command requires at least one file argument.");
        process.exit(1);
      }
      
      // Validate file:slot arguments early
      try {
        const transfers = parseFileSlotArgs(args); // Already validated above
        if (transfers.length === 0) {
          console.error("Error: No valid file arguments found.");
          process.exit(1);
        }
      } catch (error) {
        console.error(`Error: ${error.message}`);
        process.exit(1);
      }
    } else if (command === 'flash') {
      const file = process.argv[3];
      if (!file) {
        console.error("Error: 'flash' command requires a firmware .uf2 file argument.");
        process.exit(1);
      }
      if (!fs.existsSync(file)) {
        console.error(`Error: File not found: ${file}`);
        process.exit(1);
      }
    } else if (command === 'set-sequencer') {
      const file = process.argv[3];
      if (!file) {
        console.error("Error: 'set-sequencer' command requires a JSON state file argument (or '-' for stdin).");
        process.exit(1);
      }
      if (file !== '-' && !fs.existsSync(file)) {
        console.error(`Error: File not found: ${file}`);
        process.exit(1);
      }
    } else if (command === 'get-setting') {
      const name = process.argv[3];
      if (name) {
        findSetting(name); // Throws with a helpful message for unknown names
      }
    } else if (command === 'set-setting') {
      const name = process.argv[3];
      const value = process.argv[4];
      if (!name || value === undefined) {
        console.error("Error: 'set-setting' requires a setting name and a value.");
        process.exit(1);
      }
      findSetting(name); // Throws with a helpful message for unknown names
    } else if (command !== 'version' && command !== 'storage' && command !== 'format' &&
               command !== 'reboot-bootloader' && command !== 'identity' &&
               command !== 'get-sequencer') {
      console.error(`Error: Unknown command '${command}'. Use 'send', 'version', 'storage', 'format', 'reboot-bootloader', 'identity', 'flash', 'get-sequencer', 'set-sequencer', 'get-setting', or 'set-setting'.`);
      process.exit(1);
    }

    // Initialize MIDI connection after arguments are validated
    console.error("Initializing MIDI connection...");
    const midi_ports = find_dato_drum();
    if (!midi_ports) {
      console.error("Failed to initialize MIDI. Exiting.");
      process.exit(1);
    }
    
    activeMidiOutput = midi_ports.output;
    activeMidiInput = midi_ports.input;
    
    // Attach MIDI message handler
    activeMidiInput.on('message', handleMidiMessage);

    if (command === 'send') {
      const args = process.argv.slice(3).filter(arg => arg !== '--verbose' && arg !== '-v' && !arg.startsWith('--stall-after='));
      const transfers = parseFileSlotArgs(args); // Already validated above
      
      const success = transfers.length === 1 
        ? await transferSample(transfers[0].filePath, transfers[0].slot, transfers[0].sampleRate, verbose)
        : await transferMultipleSamples(transfers, verbose);
      process.exitCode = success ? 0 : 1;
    } else if (command === 'version') {
      await get_firmware_version();
    } else if (command === 'storage') {
      await get_storage_info();
    } else if (command === 'format') {
      await format_filesystem();
    } else if (command === 'reboot-bootloader') {
      await reboot_bootloader();
    } else if (command === 'identity') {
      await test_universal_identity();
    } else if (command === 'flash') {
      await flash_firmware(process.argv[3]);
    } else if (command === 'get-sequencer') {
      await get_sequencer_state(process.argv.includes('--json'));
    } else if (command === 'set-sequencer') {
      await set_sequencer_state(process.argv[3]);
    } else if (command === 'get-setting') {
      await get_setting(process.argv[3]);
    } else if (command === 'set-setting') {
      await set_setting(process.argv[3], process.argv[4]);
    }
  } catch (error) {
    console.error(`\nError: ${error.message}`);
    process.exitCode = 1;
  } finally {
    // Force exit immediately. The closePort() call from the midi library, 
    // specifically for the input port, can hang indefinitely. The OS will 
    // reclaim the MIDI resources when the process terminates.
    process.exit();
  }
}

main();
