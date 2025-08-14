#!/usr/bin/env node
/**
 * MIDI Sample Dump Standard (SDS) sender for drum firmware
 * 
 * This implements a minimal subset of the SDS specification to transfer
 * 16-bit PCM audio samples without the padding corruption issues of the
 * custom SysEx protocol.
 * 
 * Supported features:
 * - Dump Header with basic sample metadata
 * - Data Packets with proper 16-bit sample packing (40 samples per packet)
 * - ACK/NAK handshaking with timeout fallback
 * - Checksum validation
 * 
 * Not implemented (yet):
 * - Loop point messages
 * - Multiple bit depths
 * - DUMP REQUEST
 * - WAIT/CANCEL messages
 */

const midi = require('midi');
const fs = require('fs');

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

// Initialize MIDI connection
const midi_ports = find_dato_drum();
if (!midi_ports) {
  console.error("Failed to initialize MIDI. Exiting.");
  process.exit(1);
}

const { output: activeMidiOutput, input: activeMidiInput } = midi_ports;

// Message handling
let pendingResponse = null;

activeMidiInput.on('message', (deltaTime, message) => {
  if (message[0] === SYSEX_START && message[message.length - 1] === SYSEX_END) {
    if (message.length >= 6 && 
        message[1] === MIDI_NON_REALTIME_ID && 
        message[2] === SYSEX_CHANNEL) {
      
      const messageType = message[3];
      const packetNum = message.length > 4 ? message[4] : 0;
      
      if (pendingResponse && (messageType === SDS_ACK || messageType === SDS_NAK)) {
        clearTimeout(pendingResponse.timer);
        pendingResponse.resolve({ type: messageType, packet: packetNum });
        pendingResponse = null;
      }
    }
  }
});

// Send SysEx message
function sendSysExMessage(data) {
  const message = [SYSEX_START, MIDI_NON_REALTIME_ID, SYSEX_CHANNEL, ...data, SYSEX_END];
  activeMidiOutput.sendMessage(message);
}

// Wait for ACK/NAK with timeout
function waitForResponse(timeout, packetNum = 0) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      pendingResponse = null;
      resolve({ type: 'timeout', packet: packetNum });
    }, timeout);

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

// Main sample transfer function
async function transferSample(filePath, sampleNumber, sampleRate = 44100) {
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
  
  const pcmData = fs.readFileSync(filePath);
  console.log(`Sample data: ${pcmData.length} bytes, ${sampleRate}Hz, 16-bit`);
  
  // Calculate transfer parameters
  const numSamples = Math.floor(pcmData.length / 2);
  const packetsNeeded = Math.ceil(pcmData.length / 80); // 40 samples * 2 bytes per packet
  console.log(`Transfer will require ${packetsNeeded} data packets`);
  
  try {
    // Step 1: Send Dump Header
    console.log("\n1. Sending Dump Header...");
    const header = createDumpHeader(sampleNumber, 16, sampleRate, pcmData.length);
    console.log("Header bytes:", header.length, "->", header.map(b => `0x${b.toString(16).padStart(2, '0')}`).join(' '));
    sendSysExMessage(header);
    
    const headerResponse = await waitForResponse(HEADER_TIMEOUT);
    if (headerResponse.type === SDS_NAK) {
      console.log("Header NAK received, retrying...");
      sendSysExMessage(header);
      const retryResponse = await waitForResponse(HEADER_TIMEOUT);
      if (retryResponse.type !== SDS_ACK && retryResponse.type !== 'timeout') {
        throw new Error("Header rejected twice");
      }
    }
    
    let handshaking = headerResponse.type === SDS_ACK;
    console.log(`Header ${headerResponse.type === SDS_ACK ? 'ACK' : headerResponse.type} - ${handshaking ? 'handshaking mode' : 'non-handshaking mode'}`);
    
    // Step 2: Send Data Packets
    console.log("\n2. Sending data packets...");
    let packetNum = 0;
    let offset = 0;
    let successfulPackets = 0;
    
    while (offset < pcmData.length) {
      const packet = createDataPacket(packetNum & 0x7F, pcmData, offset);
      if (packetNum < 5) { // Debug first few packets
        console.log(`Packet ${packetNum} size:`, packet.length, "checksum:", `0x${packet[packet.length-1].toString(16)}`);
      }
      sendSysExMessage(packet);
      
      if (handshaking) {
        const response = await waitForResponse(PACKET_TIMEOUT, packetNum & 0x7F);
        
        if (response.type === SDS_ACK) {
          successfulPackets++;
          offset += 80; // Move to next 40 samples (80 bytes)
          packetNum++;
        } else if (response.type === SDS_NAK) {
          console.log(`Packet ${packetNum & 0x7F} NAK - retrying`);
          continue; // Retry same packet
        } else {
          // Timeout - assume non-handshaking mode
          console.log(`Packet ${packetNum & 0x7F} timeout - continuing without handshaking`);
          successfulPackets++;
          offset += 80;
          packetNum++;
          // Switch to non-handshaking mode for remaining packets
          handshaking = false;
        }
      } else {
        // Non-handshaking mode
        successfulPackets++;
        offset += 80;
        packetNum++;
      }
      
      // Progress indicator
      const progress = Math.min(100, (offset / pcmData.length) * 100);
      const bar_length = 40;
      const filled_length = Math.round(bar_length * (progress / 100));
      const bar = '█'.repeat(filled_length) + '░'.repeat(bar_length - filled_length);
      
      if (process.stdout.clearLine) {
        process.stdout.clearLine(0);
        process.stdout.cursorTo(0);
        process.stdout.write(`Progress: ${Math.round(progress)}% |${bar}| Packet ${packetNum}`);
      } else {
        // Fallback for environments without clearLine
        console.log(`Progress: ${Math.round(progress)}% |${bar}| Packet ${packetNum}`);
      }
    }
    
    console.log(`\n\n✓ Transfer complete: ${successfulPackets} packets sent`);
    return true;
    
  } catch (error) {
    console.error(`\n✗ Transfer failed: ${error.message}`);
    return false;
  }
}

// Command line interface
const command = process.argv[2];
const sourcePath = process.argv[3];
const sampleNumber = process.argv[4] ? parseInt(process.argv[4], 10) : null;
const sampleRate = process.argv[5] ? parseInt(process.argv[5]) : 44100;

if (!command) {
  console.log("SDS Sample Sender - MIDI Sample Dump Standard implementation");
  console.log("");
  console.log("Usage:");
  console.log("  sds_sender.js send <source_path> <sample_number> [sample_rate]");
  console.log("");
  console.log("Arguments:");
  console.log("  source_path    - Path to the audio file to upload");
  console.log("  sample_number  - Target sample slot (0-127)");
  console.log("  sample_rate    - Sample rate in Hz (default: 44100)");
  console.log("");
  console.log("Examples:");
  console.log("  sds_sender.js send sine440.pcm 0         # Upload to slot 0 (/00.pcm)");
  console.log("  sds_sender.js send kick.wav 1 48000     # Upload to slot 1 (/01.pcm)");
  console.log("  sds_sender.js send snare.pcm 15          # Upload to slot 15 (/15.pcm)");
  console.log("");
  process.exit(1);
}

// Cleanup handler
process.on('SIGINT', () => {
  console.log('\n\nClosing MIDI ports and exiting...');
  activeMidiOutput.closePort();
  activeMidiInput.closePort();
  process.exit(0);
});

// Main execution
async function main() {
  try {
    if (command === 'send') {
      if (!sourcePath || sampleNumber === null) {
        console.error("Error: 'send' command requires source path and sample number.");
        process.exit(1);
      }
      
      const success = await transferSample(sourcePath, sampleNumber, sampleRate);
      process.exitCode = success ? 0 : 1;
    } else {
      console.error(`Error: Unknown command '${command}'. Use 'send'.`);
      process.exit(1);
    }
  } catch (error) {
    console.error(`\nError: ${error.message}`);
    process.exitCode = 1;
  } finally {
    activeMidiOutput.closePort();
    activeMidiInput.closePort();
  }
}

main();