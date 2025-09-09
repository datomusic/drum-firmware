/**
 * SDS (Sample Dump Standard) Protocol Functions
 * 
 * This module contains the core SDS protocol functions for MIDI Sample Dump Standard.
 * All functions are platform-agnostic and can be used in different environments.
 */

// SDS Constants
const SDS_DUMP_HEADER = 0x01;
const SDS_DATA_PACKET = 0x02;
const SDS_DUMP_REQUEST = 0x03;
const SDS_ACK = 0x7F;
const SDS_NAK = 0x7E;
const SDS_CANCEL = 0x7D;
const SDS_WAIT = 0x7C;

// MIDI Constants  
const MIDI_NON_REALTIME_ID = 0x7E;
const SYSEX_START = 0xF0;
const SYSEX_END = 0xF7;
const SYSEX_CHANNEL = 0x65; // DRUM device channel

// Response Types
const RESPONSE_TYPES = {
  ACK: 'ack',
  NAK: 'nak', 
  WAIT: 'wait',
  CANCEL: 'cancel',
  TIMEOUT: 'timeout'
};

/**
 * Convert sample rate to SDS period format (3-byte nanosecond period)
 * @param {number} sampleRate - Sample rate in Hz
 * @returns {number[]} 3-byte array representing nanosecond period
 */
function sampleRateToPeriod(sampleRate) {
  const periodNs = Math.round(1000000000 / sampleRate);
  return [
    periodNs & 0x7F,           // bits 0-6
    (periodNs >> 7) & 0x7F,    // bits 7-13
    (periodNs >> 14) & 0x7F    // bits 14-20
  ];
}

/**
 * Convert length to SDS 3-byte format (word count, where word = 2 bytes)
 * @param {number} byteLength - Length in bytes
 * @returns {number[]} 3-byte array representing word count
 */
function lengthToSdsFormat(byteLength) {
  const wordLength = Math.floor(byteLength / 2); // SDS uses word count
  return [
    wordLength & 0x7F,         // bits 0-6
    (wordLength >> 7) & 0x7F,  // bits 7-13
    (wordLength >> 14) & 0x7F  // bits 14-20
  ];
}

/**
 * Create SDS Dump Header message
 * @param {number} sampleNum - Sample slot number (0-127)
 * @param {number} bitDepth - Bit depth (usually 16)
 * @param {number} sampleRate - Sample rate in Hz
 * @param {number} sampleLength - Sample length in bytes
 * @returns {number[]} SDS dump header message bytes
 */
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

/**
 * Pack 16-bit sample into SDS 3-byte format
 * @param {number} sample - 16-bit signed sample value
 * @returns {number[]} 3-byte array representing packed sample
 */
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

/**
 * Calculate XOR checksum for data packet
 * @param {number} packetNum - Packet number
 * @param {number[]} dataBytes - Data bytes to checksum
 * @returns {number} Calculated checksum byte
 */
function calculateChecksum(packetNum, dataBytes) {
  let checksum = MIDI_NON_REALTIME_ID ^ SYSEX_CHANNEL ^ SDS_DATA_PACKET ^ packetNum;
  for (const byte of dataBytes) {
    checksum ^= byte;
  }
  return checksum & 0x7F; // Clear high bit
}

/**
 * Create SDS Data Packet message
 * @param {number} packetNum - Packet number (0-127)
 * @param {Buffer|ArrayBuffer} pcmData - PCM audio data
 * @param {number} offset - Offset into PCM data
 * @returns {number[]} SDS data packet message bytes
 */
function createDataPacket(packetNum, pcmData, offset) {
  const dataBytes = [];
  
  // Pack up to 40 16-bit samples (120 bytes)
  for (let i = 0; i < 40; i++) {
    const sampleOffset = offset + (i * 2);
    if (sampleOffset + 1 < pcmData.length) {
      // Read 16-bit little-endian sample
      let sample;
      if (pcmData.readInt16LE) {
        // Node.js Buffer
        sample = pcmData.readInt16LE(sampleOffset);
      } else {
        // ArrayBuffer/TypedArray
        const view = new DataView(pcmData, sampleOffset, 2);
        sample = view.getInt16(0, true); // little-endian
      }
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

/**
 * Check if message is an SDS message
 * @param {number[]|Uint8Array} message - MIDI message bytes
 * @returns {boolean} True if message is SDS format
 */
function isSdsMessage(message) {
  return message[0] === SYSEX_START && 
         message[message.length - 1] === SYSEX_END &&
         message.length >= 6 &&
         message[1] === MIDI_NON_REALTIME_ID && 
         message[2] === SYSEX_CHANNEL;
}

/**
 * Parse SDS response message
 * @param {number[]|Uint8Array} message - MIDI message bytes
 * @returns {Object|null} Parsed response or null if not SDS response
 */
function parseSdsResponse(message) {
  if (!isSdsMessage(message)) {
    return null;
  }
  
  const messageType = message[3];
  const packetNum = message.length > 4 ? message[4] : 0;
  
  let type;
  switch (messageType) {
    case SDS_ACK:
      type = RESPONSE_TYPES.ACK;
      break;
    case SDS_NAK:
      type = RESPONSE_TYPES.NAK;
      break;
    case SDS_WAIT:
      type = RESPONSE_TYPES.WAIT;
      break;
    case SDS_CANCEL:
      type = RESPONSE_TYPES.CANCEL;
      break;
    default:
      return null;
  }
  
  return { type, packet: packetNum, raw: message };
}

/**
 * Create SDS Cancel message
 * @param {number} sampleNum - Sample number to cancel
 * @returns {number[]} SDS cancel message bytes
 */
function createCancelMessage(sampleNum) {
  const sampleNumBytes = [sampleNum & 0x7F, (sampleNum >> 7) & 0x7F];
  return [SDS_CANCEL, ...sampleNumBytes];
}

/**
 * Wrap message in SysEx envelope for SDS protocol
 * @param {number[]} data - SDS message data
 * @returns {number[]} Complete SysEx message
 */
function wrapSdsMessage(data) {
  return [SYSEX_START, MIDI_NON_REALTIME_ID, SYSEX_CHANNEL, ...data, SYSEX_END];
}

module.exports = {
  // Core protocol functions
  sampleRateToPeriod,
  lengthToSdsFormat,
  createDumpHeader,
  pack16BitSample,
  calculateChecksum,
  createDataPacket,
  createCancelMessage,
  wrapSdsMessage,
  
  // Message parsing
  isSdsMessage,
  parseSdsResponse,
  
  // Constants
  SDS_DUMP_HEADER,
  SDS_DATA_PACKET,
  SDS_DUMP_REQUEST,
  SDS_ACK,
  SDS_NAK,
  SDS_CANCEL,
  SDS_WAIT,
  MIDI_NON_REALTIME_ID,
  SYSEX_START,
  SYSEX_END,
  SYSEX_CHANNEL,
  RESPONSE_TYPES
};