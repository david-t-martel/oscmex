/**
 * Represents the state of a hardware device
 */
export enum DeviceConnectionStatus {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  ERROR
}

export interface DeviceInfo {
  id: string;
  name: string;
  type: string;
  firmwareVersion?: string;
  lastConnected?: Date;
}

export class DeviceState {
  private _status: DeviceConnectionStatus;
  private _info: DeviceInfo;
  private _lastError?: Error;
  private _metadata: Record<string, any>;

  constructor(info: DeviceInfo) {
    this._status = DeviceConnectionStatus.DISCONNECTED;
    this._info = info;
    this._metadata = {};
  }

  get id(): string {
    return this._info.id;
  }

  get name(): string {
    return this._info.name;
  }

  get type(): string {
    return this._info.type;
  }

  get status(): DeviceConnectionStatus {
    return this._status;
  }

  get info(): DeviceInfo {
    return { ...this._info };
  }

  get lastError(): Error | undefined {
    return this._lastError;
  }

  get isConnected(): boolean {
    return this._status === DeviceConnectionStatus.CONNECTED;
  }

  get metadata(): Record<string, any> {
    return { ...this._metadata };
  }

  /**
   * Updates the connection status
   */
  updateStatus(status: DeviceConnectionStatus, error?: Error): void {
    this._status = status;

    if (error) {
      this._lastError = error;
    } else if (status === DeviceConnectionStatus.CONNECTED) {
      this._info.lastConnected = new Date();
      this._lastError = undefined;
    }
  }

  /**
   * Updates device metadata
   */
  setMetadata(key: string, value: any): void {
    this._metadata[key] = value;
  }

  /**
   * Updates device info
   */
  updateInfo(info: Partial<DeviceInfo>): void {
    this._info = { ...this._info, ...info };
  }
}
