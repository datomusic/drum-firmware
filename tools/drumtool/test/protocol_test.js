/**
 * Basic tests for SDS protocol functions
 * 
 * These tests verify the core protocol functions work correctly.
 * Run with: node test/protocol_test.js
 */

const assert = require('assert');
const { 
  sampleRateToPeriod,
  lengthToSdsFormat,
  createDumpHeader,
  pack16BitSample,
  calculateChecksum,
  createDataPacket,
  isSdsMessage,
  parseSdsResponse,
  wrapSdsMessage,
  RESPONSE_TYPES
} = require('../src/protocol/sds_protocol');

function runTests() {
  console.log('Running SDS protocol tests...\n');

  // Test sample rate to period conversion
  console.log('Testing sampleRateToPeriod...');
  const period44100 = sampleRateToPeriod(44100);
  console.log(`44100Hz → ${period44100} (should be ~22676 ns)`);
  assert(Array.isArray(period44100) && period44100.length === 3);
  
  const period48000 = sampleRateToPeriod(48000);
  console.log(`48000Hz → ${period48000} (should be ~20833 ns)`);
  assert(Array.isArray(period48000) && period48000.length === 3);

  // Test length to SDS format conversion
  console.log('\nTesting lengthToSdsFormat...');
  const length1024 = lengthToSdsFormat(1024);
  console.log(`1024 bytes → ${length1024} (should be 512 words)`);
  assert(Array.isArray(length1024) && length1024.length === 3);

  // Test pack16BitSample
  console.log('\nTesting pack16BitSample...');
  const packedSample = pack16BitSample(0);
  console.log(`Sample 0 → ${packedSample} (should be [64, 0, 0])`);
  assert(Array.isArray(packedSample) && packedSample.length === 3);
  assert.deepStrictEqual(packedSample, [64, 0, 0]);

  const packedMax = pack16BitSample(32767);
  console.log(`Sample 32767 → ${packedMax} (should be [127, 127, 96])`);
  assert.deepStrictEqual(packedMax, [127, 127, 96]);

  // Test createDumpHeader
  console.log('\nTesting createDumpHeader...');
  const header = createDumpHeader(0, 16, 44100, 1024);
  console.log(`Header length: ${header.length} (should be 17)`);
  assert(header.length === 17);
  assert(header[0] === 0x01); // SDS_DUMP_HEADER

  // Test wrapSdsMessage
  console.log('\nTesting wrapSdsMessage...');
  const wrapped = wrapSdsMessage([0x01, 0x00, 0x00]);
  console.log(`Wrapped message: ${wrapped.map(b => `0x${b.toString(16)}`).join(' ')}`);
  assert(wrapped[0] === 0xF0); // SYSEX_START
  assert(wrapped[wrapped.length - 1] === 0xF7); // SYSEX_END

  // Test isSdsMessage
  console.log('\nTesting isSdsMessage...');
  assert(isSdsMessage(wrapped) === true);
  assert(isSdsMessage([0xF0, 0x7E, 0x65, 0x7F, 0x00, 0xF7]) === true); // ACK
  assert(isSdsMessage([0xF0, 0x43, 0x12, 0x00, 0x01, 0xF7]) === false); // Not SDS

  // Test parseSdsResponse  
  console.log('\nTesting parseSdsResponse...');
  const ackMessage = [0xF0, 0x7E, 0x65, 0x7F, 0x00, 0xF7];
  const parsedAck = parseSdsResponse(ackMessage);
  console.log(`Parsed ACK: ${JSON.stringify(parsedAck)}`);
  assert(parsedAck && parsedAck.type === RESPONSE_TYPES.ACK);

  const nakMessage = [0xF0, 0x7E, 0x65, 0x7E, 0x00, 0xF7];
  const parsedNak = parseSdsResponse(nakMessage);
  console.log(`Parsed NAK: ${JSON.stringify(parsedNak)}`);
  assert(parsedNak && parsedNak.type === RESPONSE_TYPES.NAK);

  // Test createDataPacket (with mock PCM data)
  console.log('\nTesting createDataPacket...');
  const mockPcmData = Buffer.alloc(160, 0); // 80 samples worth of data
  mockPcmData.writeInt16LE(1000, 0);
  mockPcmData.writeInt16LE(-1000, 2);
  
  const dataPacket = createDataPacket(0, mockPcmData, 0);
  console.log(`Data packet length: ${dataPacket.length} (should be 123)`);
  assert(dataPacket.length === 123);
  assert(dataPacket[0] === 0x02); // SDS_DATA_PACKET
  assert(dataPacket[1] === 0x00); // Packet number

  console.log('\n✅ All protocol tests passed!');
}

// Run tests if this file is executed directly
if (require.main === module) {
  try {
    runTests();
  } catch (error) {
    console.error('\n❌ Test failed:', error.message);
    console.error(error.stack);
    process.exit(1);
  }
}