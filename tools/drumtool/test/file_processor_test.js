/**
 * Tests for FileProcessor class
 * 
 * These tests verify file processing functionality using mock implementations.
 * Run with: node test/file_processor_test.js
 */

const assert = require('assert');
const { FileProcessor } = require('../src/core/file_processor');

// Mock FileSystem implementation for testing
class MockFileSystem {
  constructor() {
    this.files = new Map();
  }
  
  addFile(path, data) {
    this.files.set(path, data);
  }
  
  async exists(filePath) {
    return this.files.has(filePath);
  }
  
  async readFile(filePath) {
    if (!this.files.has(filePath)) {
      throw new Error(`File not found: ${filePath}`);
    }
    return this.files.get(filePath);
  }
  
  async getFileSize(filePath) {
    const data = await this.readFile(filePath);
    return data.length;
  }
  
  getFileExtension(filePath) {
    const lastDot = filePath.lastIndexOf('.');
    return lastDot === -1 ? '' : filePath.substring(lastDot).toLowerCase();
  }
  
  getFileName(filePath) {
    const lastSlash = Math.max(filePath.lastIndexOf('/'), filePath.lastIndexOf('\\'));
    return lastSlash === -1 ? filePath : filePath.substring(lastSlash + 1);
  }
}

// Mock UserInterface implementation for testing  
class MockUserInterface {
  constructor() {
    this.messages = [];
  }
  
  async info(message) {
    this.messages.push({ type: 'info', message });
  }
  
  async error(message) {
    this.messages.push({ type: 'error', message });
  }
  
  async warn(message) {
    this.messages.push({ type: 'warn', message });
  }
  
  async confirm(message, defaultValue = false) {
    this.messages.push({ type: 'confirm', message, defaultValue });
    return defaultValue; // Always return default for testing
  }
  
  async updateProgress(progress) {
    this.messages.push({ type: 'progress', progress });
  }
  
  async clearProgress() {
    this.messages.push({ type: 'clearProgress' });
  }
  
  async complete(message, details = null) {
    this.messages.push({ type: 'complete', message, details });
  }
}

function runTests() {
  console.log('Running FileProcessor tests...\n');

  const mockFs = new MockFileSystem();
  const mockUi = new MockUserInterface();
  const processor = new FileProcessor(mockFs, mockUi);

  // Test validateSlot
  console.log('Testing validateSlot...');
  assert(processor.validateSlot(0) === true);
  assert(processor.validateSlot(127) === true);
  assert(processor.validateSlot(-1) === false);
  assert(processor.validateSlot(128) === false);
  assert(processor.validateSlot('0') === false);
  console.log('✓ validateSlot works correctly');

  // Test generateTargetFilename
  console.log('\nTesting generateTargetFilename...');
  assert(processor.generateTargetFilename(0) === '00.pcm');
  assert(processor.generateTargetFilename(5) === '05.pcm');
  assert(processor.generateTargetFilename(127) === '127.pcm');
  
  try {
    processor.generateTargetFilename(-1);
    assert(false, 'Should have thrown error for invalid slot');
  } catch (error) {
    assert(error.message.includes('Invalid slot number'));
  }
  console.log('✓ generateTargetFilename works correctly');

  // Test parseFileSlotArg
  console.log('\nTesting parseFileSlotArg...');
  const parsed = processor.parseFileSlotArg('test.wav:5');
  assert(parsed.filePath === 'test.wav');
  assert(parsed.slot === 5);
  
  try {
    processor.parseFileSlotArg('invalid');
    assert(false, 'Should have thrown error for invalid format');
  } catch (error) {
    assert(error.message.includes('Invalid format'));
  }
  
  try {
    processor.parseFileSlotArg('test.wav:invalid');
    assert(false, 'Should have thrown error for invalid slot');
  } catch (error) {
    assert(error.message.includes('Invalid slot number'));
  }
  console.log('✓ parseFileSlotArg works correctly');

  // Test isSampleRate
  console.log('\nTesting isSampleRate...');
  assert(processor.isSampleRate('44100') === true);
  assert(processor.isSampleRate('48000') === true);
  assert(processor.isSampleRate('999') === false); // Too low
  assert(processor.isSampleRate('test.wav:0') === false); // Contains colon
  assert(processor.isSampleRate('abc') === false); // Not a number
  console.log('✓ isSampleRate works correctly');

  // Test parseTransferArgs - explicit mode
  console.log('\nTesting parseTransferArgs (explicit mode)...');
  const explicitArgs = ['kick.wav:0', 'snare.wav:1', '48000', '--verbose'];
  const explicitResult = processor.parseTransferArgs(explicitArgs);
  
  assert(explicitResult.mode === 'explicit');
  assert(explicitResult.sampleRate === 48000);
  assert(explicitResult.transfers.length === 2);
  assert(explicitResult.transfers[0].filePath === 'kick.wav');
  assert(explicitResult.transfers[0].slot === 0);
  assert(explicitResult.transfers[1].filePath === 'snare.wav');
  assert(explicitResult.transfers[1].slot === 1);
  console.log('✓ parseTransferArgs (explicit mode) works correctly');

  // Test parseTransferArgs - auto mode
  console.log('\nTesting parseTransferArgs (auto mode)...');
  const autoArgs = ['kick.wav', 'snare.wav', 'hat.wav'];
  const autoResult = processor.parseTransferArgs(autoArgs);
  
  assert(autoResult.mode === 'auto');
  assert(autoResult.sampleRate === 44100); // Default
  assert(autoResult.transfers.length === 3);
  assert(autoResult.transfers[0].slot === 0);
  assert(autoResult.transfers[1].slot === 1);
  assert(autoResult.transfers[2].slot === 2);
  console.log('✓ parseTransferArgs (auto mode) works correctly');

  // Test mixed format error
  console.log('\nTesting mixed format error...');
  try {
    processor.parseTransferArgs(['kick.wav:0', 'snare.wav']); // Mixed formats
    assert(false, 'Should have thrown error for mixed formats');
  } catch (error) {
    assert(error.message.includes('Mixed formats not allowed'));
  }
  console.log('✓ Mixed format error handling works correctly');

  // Test processAudioFile with PCM file
  console.log('\nTesting processAudioFile (PCM)...');
  const pcmData = Buffer.alloc(1024);
  for (let i = 0; i < 512; i++) {
    pcmData.writeInt16LE(Math.floor(Math.sin(i * 0.1) * 1000), i * 2);
  }
  mockFs.addFile('/test/sample.pcm', pcmData);
  
  processor.processAudioFile('/test/sample.pcm', 44100, false).then(result => {
    assert(result.pcmData.length === 1024);
    assert(result.sampleRate === 44100);
    assert(result.bitDepth === 16);
    assert(result.channels === 1);
    assert(result.fileName === 'sample.pcm');
    assert(result.fileExtension === '.pcm');
    console.log('✓ processAudioFile (PCM) works correctly');
  }).catch(error => {
    console.error('PCM processing test failed:', error);
  });

  console.log('\n✅ All FileProcessor tests passed!');
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