declare module 'midi' {
  export class Input {
    constructor();
    on(event: string, callback: (deltaTime: number, message: number[]) => void): void;
    openPort(port: number): void;
    closePort(): void;
    getPortCount(): number;
    getPortName(port: number): string;
    ignoreTypes(sysex: boolean, timing: boolean, activeSensing: boolean): void;
    isPortOpen(): boolean;
  }
  export class Output {
    constructor();
    openPort(port: number): void;
    closePort(): void;
    getPortCount(): number;
    getPortName(port: number): string;
    sendMessage(message: number[]): void;
    isPortOpen(): boolean;
  }
  export const input: typeof Input;
  export const output: typeof Output;
}