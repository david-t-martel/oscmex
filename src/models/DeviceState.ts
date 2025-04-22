/**
 * Represents the current state of a physical device in the system
 */
export enum DeviceStatus {
  ONLINE = 'online',
  OFFLINE = 'offline',
  ERROR = 'error',
  INITIALIZING = 'initializing'
}

export interface DeviceStateOptions {
  id: string;
  name: string;
  type: string;
  status?: DeviceStatus;
  lastUpdated?: Date;
  metadata?: Record<string, any>;
}

export class DeviceState {
  public id: string;
  public name: string;
  public type: string;
  public status: DeviceStatus;
  public lastUpdated: Date;
  public metadata: Record<string, any>;

  constructor(options: DeviceStateOptions) {
    this.id = options.id;
    this.name = options.name;
    this.type = options.type;
    this.status = options.status || DeviceStatus.OFFLINE;
    this.lastUpdated = options.lastUpdated || new Date();
    this.metadata = options.metadata || {};
  }

  /**
   * Updates the status of the device
   * @param status The new status of the device
   */
  public updateStatus(status: DeviceStatus): void {
    this.status = status;
    this.lastUpdated = new Date();
  }

  /**
   * Checks if the device is currently online
   */
  public isOnline(): boolean {
    return this.status === DeviceStatus.ONLINE;
  }

  /**
   * Updates device metadata
   * @param metadata The metadata to update
   */
  public updateMetadata(metadata: Record<string, any>): void {
    this.metadata = {
      ...this.metadata,
      ...metadata
    };
    this.lastUpdated = new Date();
  }

  /**
   * Serializes the device state to a plain object
   */
  public toJSON(): Record<string, any> {
    return {
      id: this.id,
      name: this.name,
      type: this.type,
      status: this.status,
      lastUpdated: this.lastUpdated,
      metadata: this.metadata
    };
  }
}
