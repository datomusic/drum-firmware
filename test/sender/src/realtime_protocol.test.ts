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
    // Probe the device once to ensure it's responsive before starting tests.
    await probeDevice();
  });

  afterAll(() => {
    if (transport && transport.isOpen()) {
      transport.disconnect();
    }
  });

  const collectMidiMessages = async (duration: number): Promise<number[][]> => {
    const messages: number[][] = [];
    const messageHandler = (message: Uint8Array) => {
      messages.push(Array.from(message));
    };

    transport.onMessage(messageHandler);
    await new Promise(resolve => setTimeout(resolve, duration));
    transport.removeOnMessage(messageHandler); // Clean up the listener

    return messages;
  };

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

  test('should start sending MIDI clocks on START message', async () => {
    protocol.detach();
    await transport.sendMessage(new Uint8Array([0xFA])); // START
    const messages = await collectMidiMessages(200);
    console.log(messages);
    protocol.attach();
    expect(messages.some(msg => msg[0] === 0xF8)).toBe(true);
  });

  test('should stop sending MIDI clocks on STOP message', async () => {
    protocol.detach();
    await transport.sendMessage(new Uint8Array([0xFA])); // Ensure clocks are running
    await new Promise(resolve => setTimeout(resolve, 100));

    await transport.sendMessage(new Uint8Array([0xFC])); // STOP
    const messages = await collectMidiMessages(200);
    protocol.attach();
    expect(messages.every(msg => msg[0] !== 0xF8)).toBe(true);
  });

  test('should resume sending MIDI clocks on CONTINUE message', async () => {
    protocol.detach();
    await transport.sendMessage(new Uint8Array([0xFA])); // START
    await new Promise(resolve => setTimeout(resolve, 100));
    await transport.sendMessage(new Uint8Array([0xFC])); // STOP
    await new Promise(resolve => setTimeout(resolve, 100));

    await transport.sendMessage(new Uint8Array([0xFB])); // CONTINUE
    const messages = await collectMidiMessages(200);
    protocol.attach();
    expect(messages.some(msg => msg[0] === 0xF8)).toBe(true);
  });

  test('should remain responsive and not send clocks after a burst of MIDI Clock messages', async () => {
    for (let i = 0; i < 48; i++) {
      await transport.sendMessage(new Uint8Array([0xF8]));
    }
    // Check that the device doesn't start sending its own clock messages
    const messages = await collectMidiMessages(200);
    expect(messages.every(msg => msg[0] !== 0xF8)).toBe(true);

    // And ensure it's still responsive
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
