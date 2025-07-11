import { NodeMidiTransport } from './NodeMidiTransport';
import { SysexProtocol } from './SysexProtocol';
import { Command } from './Command';
import * as fs from 'fs';
import * as path from 'path';

describe('SysexProtocol', () => {
  let transport: NodeMidiTransport;
  let protocol: SysexProtocol;

  beforeAll(async () => {
    transport = new NodeMidiTransport();
    await transport.connect();
    protocol = new SysexProtocol(transport);
  });

  afterAll(() => {
    if (transport && transport.isOpen()) {
      transport.disconnect();
    }
  });

  test('should transfer a file successfully', async () => {
    const filePath = path.join(__dirname, '../test-files/test.bin');
    const fileName = 'test.bin';
    const fileData = fs.readFileSync(filePath);

    await protocol.beginFileTransfer(fileName);

    const CHUNK_SIZE = 98;
    for (let i = 0; i < fileData.length; i += CHUNK_SIZE) {
      const chunk = fileData.slice(i, i + CHUNK_SIZE);
      await protocol.sendFileChunk(chunk);
    }

    await protocol.endFileTransfer();
  });

  test('should recover after a transfer is aborted mid-stream', async () => {
    const fileName = 'abort.bin';
    await protocol.beginFileTransfer(fileName);

    // Send one valid chunk to start the process
    const chunk = Buffer.from([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07]);
    await protocol.sendFileChunk(chunk);

    // The transfer is now "stuck". We will try to start a new one every second
    // until the device recovers from its own timeout.
    let recoverySuccessful = false;
    for (let i = 0; i < 10; i++) {
      try {
        console.log(`\nAttempting recovery transfer (attempt ${i + 1}/10)...`);
        const recoveryFileName = 'recovery.bin';
        await protocol.beginFileTransfer(recoveryFileName);
        await protocol.endFileTransfer();

        recoverySuccessful = true;
        console.log('Recovery transfer successful.');
        break; // Exit the loop on success
      } catch (error) {
        if (error instanceof Error && error.message.includes('Timeout waiting for ACK')) {
          // This is expected if the device hasn't timed out yet.
          console.log('Device not ready, waiting 1 second to retry...');
          await new Promise(resolve => setTimeout(resolve, 1000));
        } else {
          // Any other error should fail the test immediately.
          throw error;
        }
      }
    }

    expect(recoverySuccessful).toBe(true);
  }, 15000); // Increase jest timeout for this long-running test

  test('should reject a file with an invalid name', async () => {
    const invalidFileName = '../invalid.bin';
    await expect(protocol.beginFileTransfer(invalidFileName)).rejects.toThrow('Received NACK from device.');
  });

  // --- Optional Long-Running Tests ---
  // These tests are skipped by default. Run them with:
  // RUN_LARGE_TESTS=true npm test
  const runLargeTests = process.env.RUN_LARGE_TESTS === 'true';

  (runLargeTests ? test : test.skip)('should fail gracefully when sending a file that is too large', async () => {
    const fourMegabytes = 4 * 1024 * 1024;
    const largeFile = Buffer.alloc(fourMegabytes);
    const fileName = 'large_file.bin';

    await protocol.beginFileTransfer(fileName);

    let transferFailedAsExpected = false;
    const CHUNK_SIZE = 98;

    console.log('\nSending large file, expecting failure...');
    for (let i = 0; i < largeFile.length; i += CHUNK_SIZE) {
      const chunk = largeFile.slice(i, i + CHUNK_SIZE);
      try {
        await protocol.sendFileChunk(chunk);
        process.stdout.write(`.`); // Print a dot for each successful chunk
      } catch (error: any) {
        console.log('\nTransfer failed as expected.');
        expect(error.message).toContain('Received NACK from device.');
        transferFailedAsExpected = true;
        break; // Exit the loop on failure
      }
    }

    expect(transferFailedAsExpected).toBe(true);

    // Now, ensure the device has recovered and is ready for a new transfer.
    // We need to wait for the device to timeout and reset its internal state.
    console.log('Waiting for device to recover...');
    await new Promise(resolve => setTimeout(resolve, 5500)); // Wait for firmware timeout

    console.log('Attempting recovery transfer after large file failure...');
    const recoveryFileName = 'recovery_after_large.bin';
    await protocol.beginFileTransfer(recoveryFileName);
    await protocol.endFileTransfer();
    console.log('Recovery successful.');

  }, 60000); // 60-second timeout for this long test

  (runLargeTests ? test : test.skip)('should receive a NACK when writing to a full filesystem', async () => {
    // 1. Fill the filesystem with a large file.
    const fourMegabytes = 4 * 1024 * 1024;
    const largeFile = Buffer.alloc(fourMegabytes);
    const largeFileName = 'large_file_to_fill_fs.bin';

    await protocol.beginFileTransfer(largeFileName);

    let transferFailedAsExpected = false;
    const CHUNK_SIZE = 98;

    for (let i = 0; i < largeFile.length; i += CHUNK_SIZE) {
      const chunk = largeFile.slice(i, i + CHUNK_SIZE);
      try {
        await protocol.sendFileChunk(chunk);
      } catch (error: any) {
        transferFailedAsExpected = true;
        break;
      }
    }
    expect(transferFailedAsExpected).toBe(true);

    // 2. Attempt to write a small file.
    const smallFileName = 'small_file.bin';
    await expect(protocol.beginFileTransfer(smallFileName)).rejects.toThrow('Received NACK from device.');
  }, 60000);

  test('should get storage info', async () => {
    const info = await protocol.getStorageInfo();
    expect(info.total).toBeGreaterThan(0);
    expect(info.free).toBeGreaterThan(0);
    expect(info.free).toBeLessThanOrEqual(info.total);
  });
});
