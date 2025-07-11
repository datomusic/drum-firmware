
export interface IMidiTransport {
  connect(): Promise<void>;
  disconnect(): void;
  sendMessage(data: Uint8Array): Promise<void>;
  onMessage(callback: (data: Uint8Array) => void): void;
  isOpen(): boolean;
}
