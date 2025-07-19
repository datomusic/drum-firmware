import { NodeMidiTransport } from './NodeMidiTransport';
import { SysexProtocol } from './SysexProtocol';

describe('MIDI Realtime Message Handling', () => {
  let transport: NodeMidiTransport;
  let protocol: SysexProtocol;

  // Increase Jest's default timeout for these hardware tests
  jest.setTimeout(10000);

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

  const probeDevice = async () => {
    try {
      const version = await protocol.getFirmwareVersion();
      expect(version.major).toBeDefined();
      expect(version.minor).toBeDefined();
      expect(version.patch).toBeDefined();
    } catch (e) {
      // Explicitly fail the test if the probe fails.
      throw new Error('Device did not respond to firmware version request.');
    }
  };

  test('should remain responsive after a MIDI Start message', async () => {
    await transport.sendMessage(new Uint8Array([0xFA]));
    await new Promise(resolve => setTimeout(resolve, 100)); // Wait for processing
    await probeDevice();
  });

  test('should remain responsive after a MIDI Stop message', async () => {
    await transport.sendMessage(new Uint8Array([0xFC]));
    await new Promise(resolve => setTimeout(resolve, 100));
    await probeDevice();
  });

  test('should remain responsive after a MIDI Continue message', async () => {
    await transport.sendMessage(new Uint8Array([0xFB]));
    await new Promise(resolve => setTimeout(resolve, 100));
    await probeDevice();
  });

  test('should remain responsive after a burst of MIDI Clock messages', async () => {
    for (let i = 0; i < 48; i++) {
      await transport.sendMessage(new Uint8Array([0xF8]));
    }
    await new Promise(resolve => setTimeout(resolve, 100));
    await probeDevice();
  });

  test('should remain responsive after a typical play/stop sequence', async () => {
    await transport.sendMessage(new Uint8Array([0xFA])); // Start
    for (let i = 0; i < 24; i++) {
      await transport.sendMessage(new Uint8Array([0xF8])); // Clocks
    }
    await transport.sendMessage(new Uint8Array([0xFC])); // Stop
    await new Promise(resolve => setTimeout(resolve, 100));
    await probeDevice();
  });
});
