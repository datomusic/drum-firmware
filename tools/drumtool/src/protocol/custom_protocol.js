/**
 * DATO DRUM Custom Protocol Functions
 * 
 * This module contains functions for DATO DRUM's custom MIDI protocol
 * for device management commands (version, format, reboot, etc.)
 */

// Custom Protocol Constants
const SYSEX_MANUFACTURER_ID = [0x00, 0x22, 0x01]; // Dato Musical Instruments
const SYSEX_DEVICE_ID = 0x65; // DRUM device ID

// Custom Protocol Commands
const REQUEST_FIRMWARE_VERSION = 0x01;
const REBOOT_BOOTLOADER = 0x0B;
const FORMAT_FILESYSTEM = 0x15;
const CUSTOM_ACK = 0x13;
const CUSTOM_NACK = 0x14;

// MIDI Constants
const SYSEX_START = 0xF0;
const SYSEX_END = 0xF7;

/**
 * Check if message is a custom protocol message
 * @param {number[]|Uint8Array} message - MIDI message bytes
 * @returns {boolean} True if message is custom protocol format
 */
function isCustomMessage(message) {
  return message[0] === SYSEX_START && 
         message[message.length - 1] === SYSEX_END &&
         message.length >= 6 &&
         message[1] === SYSEX_MANUFACTURER_ID[0] &&
         message[2] === SYSEX_MANUFACTURER_ID[1] &&
         message[3] === SYSEX_MANUFACTURER_ID[2] &&
         message[4] === SYSEX_DEVICE_ID;
}

/**
 * Parse custom protocol message
 * @param {number[]|Uint8Array} message - MIDI message bytes
 * @returns {Object|null} Parsed message or null if not custom protocol
 */
function parseCustomMessage(message) {
  if (!isCustomMessage(message) || message.length < 6) {
    return null;
  }
  
  const tag = message[5];
  const payload = message.slice(6, -1); // Remove start, header, and end bytes
  
  return { tag, payload, raw: message };
}

/**
 * Create custom protocol message
 * @param {number[]} payload - Message payload bytes
 * @returns {number[]} Complete custom protocol message
 */
function createCustomMessage(payload) {
  return [SYSEX_START, ...SYSEX_MANUFACTURER_ID, SYSEX_DEVICE_ID, ...payload, SYSEX_END];
}

/**
 * Create firmware version request message
 * @returns {number[]} Firmware version request message
 */
function createFirmwareVersionRequest() {
  return createCustomMessage([REQUEST_FIRMWARE_VERSION]);
}

/**
 * Parse firmware version response
 * @param {Object} parsedMessage - Parsed custom protocol message
 * @returns {Object|null} Firmware version info or null if not version response
 */
function parseFirmwareVersionResponse(parsedMessage) {
  if (!parsedMessage || parsedMessage.tag !== REQUEST_FIRMWARE_VERSION) {
    return null;
  }
  
  if (parsedMessage.payload.length < 3) {
    throw new Error('Invalid firmware version response format');
  }
  
  const [major, minor, patch] = parsedMessage.payload;
  return { major, minor, patch };
}

/**
 * Create format filesystem command message
 * @returns {number[]} Format filesystem command message
 */
function createFormatFilesystemCommand() {
  return createCustomMessage([FORMAT_FILESYSTEM]);
}

/**
 * Create reboot to bootloader command message
 * @returns {number[]} Reboot bootloader command message
 */
function createRebootBootloaderCommand() {
  return createCustomMessage([REBOOT_BOOTLOADER]);
}

/**
 * Check if message is an ACK response
 * @param {Object} parsedMessage - Parsed custom protocol message
 * @returns {boolean} True if message is ACK
 */
function isAckResponse(parsedMessage) {
  return parsedMessage && parsedMessage.tag === CUSTOM_ACK;
}

/**
 * Check if message is a NACK response
 * @param {Object} parsedMessage - Parsed custom protocol message
 * @returns {boolean} True if message is NACK
 */
function isNackResponse(parsedMessage) {
  return parsedMessage && parsedMessage.tag === CUSTOM_NACK;
}

module.exports = {
  // Message creation
  createCustomMessage,
  createFirmwareVersionRequest,
  createFormatFilesystemCommand,
  createRebootBootloaderCommand,
  
  // Message parsing
  isCustomMessage,
  parseCustomMessage,
  parseFirmwareVersionResponse,
  isAckResponse,
  isNackResponse,
  
  // Constants
  REQUEST_FIRMWARE_VERSION,
  REBOOT_BOOTLOADER,
  FORMAT_FILESYSTEM,
  CUSTOM_ACK,
  CUSTOM_NACK,
  SYSEX_MANUFACTURER_ID,
  SYSEX_DEVICE_ID
};