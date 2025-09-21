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
const REBOOT_BOOTLOADER = 0x0B;
const FORMAT_FILESYSTEM = 0x15;
const CUSTOM_ACK = 0x13;
const CUSTOM_NACK = 0x14;

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
    console.log(`Opened MIDI ports: ${portName}`);
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

// Send SysEx message (for SDS protocol)
function sendSysExMessage(data) {
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
    sendSysExMessage(header);
    
    const headerResponse = await waitForResponse(HEADER_TIMEOUT);
    if (headerResponse.type === SDS_NAK) {
      if (verbose) {
        console.log("Header NAK received, retrying...");
      }
      sendSysExMessage(createDumpHeader(sampleNumber, 16, finalSampleRate, pcmData.length));
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
      const packet = createDataPacket(packetNum & 0x7F, pcmData, offset);
      if (verbose && packetNum < 5) { // Debug first few packets
        console.log(`Packet ${packetNum} size:`, packet.length, "checksum:", `0x${packet[packet.length-1].toString(16)}`);
      }
      sendSysExMessage(packet);
      
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

if (!command) {
  console.log("Drumtool - DRUM Device Management Tool");
  console.log("");
  console.log("Usage:");
  console.log("  drumtool.js send <file:slot> [file:slot] ... [sample_rate] [--verbose|-v]");
  console.log("  drumtool.js version");
  console.log("  drumtool.js format");
  console.log("  drumtool.js reboot-bootloader");
  console.log("");
  console.log("Commands:");
  console.log("  send           - Transfer audio samples (WAV or raw PCM) using SDS protocol");
  console.log("  version        - Get device firmware version");
  console.log("  format         - Format device filesystem");
  console.log("  reboot-bootloader - Reboot device into bootloader mode");
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
  console.log("  drumtool.js format                            # Format filesystem");
  console.log("  drumtool.js reboot-bootloader                 # Enter bootloader mode");
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
    sendSysExMessage(cancelMessage);
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
      const args = process.argv.slice(3).filter(arg => arg !== '--verbose' && arg !== '-v');
      
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
    } else if (command !== 'version' && command !== 'format' && command !== 'reboot-bootloader') {
      console.error(`Error: Unknown command '${command}'. Use 'send', 'version', 'format', or 'reboot-bootloader'.`);
      process.exit(1);
    }

    // Initialize MIDI connection after arguments are validated
    console.log("Initializing MIDI connection...");
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
      const args = process.argv.slice(3).filter(arg => arg !== '--verbose' && arg !== '-v');
      const transfers = parseFileSlotArgs(args); // Already validated above
      
      const success = transfers.length === 1 
        ? await transferSample(transfers[0].filePath, transfers[0].slot, transfers[0].sampleRate, verbose)
        : await transferMultipleSamples(transfers, verbose);
      process.exitCode = success ? 0 : 1;
    } else if (command === 'version') {
      await get_firmware_version();
    } else if (command === 'format') {
      await format_filesystem();
    } else if (command === 'reboot-bootloader') {
      await reboot_bootloader();
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
