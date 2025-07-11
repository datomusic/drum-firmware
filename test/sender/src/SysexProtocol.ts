import { IMidiTransport } from './IMidiTransport';
import { Command } from './Command';

const SYSEX_MANUFACTURER_ID = [0x00, 0x22, 0x01];
const SYSEX_DEVICE_ID = 0x65;



export class SysexProtocol {
  private transport: IMidiTransport;
  private ackQueue: { resolve: () => void; reject: (reason?: any) => void; timer: NodeJS.Timeout }[] = [];

  constructor(transport: IMidiTransport) {
    this.transport = transport;
    this.transport.onMessage(this.handleMidiMessage.bind(this));
  }

  private handleMidiMessage(message: Uint8Array): void {
    if (message[0] === 0xF0 && message[message.length - 1] === 0xF7) {
      const isOurManufacturer = message[1] === SYSEX_MANUFACTURER_ID[0] &&
                              message[2] === SYSEX_MANUFACTURER_ID[1] &&
                              message[3] === SYSEX_MANUFACTURER_ID[2];

      if (isOurManufacturer && message[4] === SYSEX_DEVICE_ID) {
        if (message.length < 6) return;
        const tag = message[5];

        if (tag === Command.Ack) {
          const ackResolver = this.ackQueue.shift();
          if (ackResolver) {
            clearTimeout(ackResolver.timer);
            ackResolver.resolve();
          }
        } else if (tag === Command.Nack) {
          const ackResolver = this.ackQueue.shift();
          if (ackResolver) {
            clearTimeout(ackResolver.timer);
            ackResolver.reject(new Error('Received NACK from device.'));
          }
        }
      }
    }
  }

  private waitForAck(timeout = 2000): Promise<void> {
    let timer: NodeJS.Timeout;
    const promise = new Promise<void>((resolve, reject) => {
      const resolver = {
        resolve: resolve,
        reject: reject,
        timer: setTimeout(() => {
          const index = this.ackQueue.findIndex(p => p.timer === resolver.timer);
          if (index > -1) {
            this.ackQueue.splice(index, 1);
          }
          reject(new Error(`Timeout waiting for ACK after ${timeout}ms.`));
        }, timeout)
      };
      this.ackQueue.push(resolver);
    });
    return promise;
  }

  private async sendMessage(payload: number[]): Promise<void> {
    const message = [0xF0, ...SYSEX_MANUFACTURER_ID, SYSEX_DEVICE_ID, ...payload, 0xF7];
    await this.transport.sendMessage(new Uint8Array(message));
  }

  private async sendCommandAndWait(tag: Command, body: number[] = []): Promise<void> {
    const payload = [tag, ...body];
    await this.sendMessage(payload);
    await this.waitForAck();
  }

  async beginFileTransfer(fileName: string): Promise<void> {
    const encoded = new TextEncoder().encode(fileName);
    const encoded_with_null = new Uint8Array(encoded.length + 1);
    encoded_with_null.set(encoded);
    encoded_with_null[encoded.length] = 0;

    let bytes: number[] = [];
    for (let i = 0; i < encoded_with_null.length; i += 2) {
      const lower = encoded_with_null[i];
      const upper = (i + 1 < encoded_with_null.length) ? encoded_with_null[i + 1] : 0;
      bytes = bytes.concat(this.pack3_16((upper << 8) | lower));
    }

    await this.sendCommandAndWait(Command.BeginFileWrite, bytes);
  }

  async endFileTransfer(): Promise<void> {
    await this.sendCommandAndWait(Command.EndFileTransfer);
  }

  async sendFileChunk(chunk: Buffer): Promise<void> {
    const encoded_chunk = this.encode_7_to_8(chunk);
    const payload = [Command.FileBytes, ...encoded_chunk];
    await this.sendMessage(payload);
    await this.waitForAck();
  }

  private pack3_16(value: number): number[] {
    return [(value >> 14) & 0x7F, (value >> 7) & 0x7F, value & 0x7F];
  }

  private encode_7_to_8(buffer: Buffer): number[] {
    let encoded: number[] = [];
    for (let i = 0; i < buffer.length; i += 7) {
      const chunk = buffer.slice(i, i + 7);
      let msbs = 0;
      let data_bytes: number[] = [];
      for (let j = 0; j < chunk.length; j++) {
        if (chunk[j] & 0x80) {
          msbs |= (1 << j);
        }
        data_bytes.push(chunk[j] & 0x7F);
      }
      encoded.push(...data_bytes);
      for (let j = chunk.length; j < 7; j++) {
        encoded.push(0);
      }
      encoded.push(msbs);
    }
    return encoded;
  }
}
