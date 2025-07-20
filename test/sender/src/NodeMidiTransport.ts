import midi = require('midi');
import { IMidiTransport } from './IMidiTransport';

export class NodeMidiTransport implements IMidiTransport {
  private input: midi.Input;
  private output: midi.Output;
  private onMessageCallbacks: ((data: Uint8Array) => void)[] = [];

  constructor() {
    this.input = new midi.Input();
    this.output = new midi.Output();
    this.input.on('message', (deltaTime: number, message: number[]) => {
      this.onMessageCallbacks.forEach(callback => {
        callback(new Uint8Array(message));
      });
    });
  }

  async connect(): Promise<void> {
    const port = this.findPort();
    if (port === null) {
      throw new Error('Dato DRUM device not found.');
    }

    this.output.openPort(port.output);
    this.input.openPort(port.input);
    this.input.ignoreTypes(false, false, false);
  }

  disconnect(): void {
    this.output.closePort();
    this.input.closePort();
  }

  async sendMessage(data: Uint8Array): Promise<void> {
    this.output.sendMessage(Array.from(data));
  }

  onMessage(callback: (data: Uint8Array) => void): void {
    this.onMessageCallbacks.push(callback);
  }

  removeOnMessage(callback: (data: Uint8Array) => void): void {
    const index = this.onMessageCallbacks.indexOf(callback);
    if (index !== -1) {
      this.onMessageCallbacks.splice(index, 1);
    }
  }

  isOpen(): boolean {
    return this.output.isPortOpen() && this.input.isPortOpen();
  }

  private findPort(): { input: number; output: number } | null {
    const outPortCount = this.output.getPortCount();
    const inPortCount = this.input.getPortCount();
    const validPortNames = ["Pico", "DRUM"];

    let outPort = -1;
    let inPort = -1;
    let portName = "";

    for (let i = 0; i < outPortCount; i++) {
      const currentPortName = this.output.getPortName(i);
      if (validPortNames.some(name => currentPortName.includes(name))) {
        outPort = i;
        portName = currentPortName;
        break;
      }
    }

    if (outPort === -1) {
      return null;
    }

    for (let i = 0; i < inPortCount; i++) {
      if (this.input.getPortName(i).includes(portName)) {
        inPort = i;
        break;
      }
    }

    if (inPort === -1) {
      return null;
    }

    return { input: inPort, output: outPort };
  }
}
