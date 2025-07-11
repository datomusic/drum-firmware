export enum Command {
  RequestFirmwareVersion = 0x01,
  RequestSerialNumber = 0x02,
  RequestStorageInfo = 0x03,
  StorageInfoResponse = 0x04,
  RebootBootloader = 0x0B,
  BeginFileWrite = 0x10,
  FileBytes = 0x11,
  EndFileTransfer = 0x12,
  Ack = 0x13,
  Nack = 0x14,
  FormatFilesystem = 0x15,
}
