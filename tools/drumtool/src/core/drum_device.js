/**
 * DrumDevice - DATO DRUM Device Communication
 * 
 * This class handles all communication with DATO DRUM devices,
 * including both SDS protocol and custom device management commands.
 */

const { parseSdsResponse, wrapSdsMessage } = require('../protocol/sds_protocol');
const { 
  parseCustomMessage, 
  parseFirmwareVersionResponse,
  createFirmwareVersionRequest,
  createFormatFilesystemCommand,
  createRebootBootloaderCommand,
  isAckResponse,
  isNackResponse 
} = require('../protocol/custom_protocol');

class DrumDeviceError extends Error {
  constructor(message, code = 'DEVICE_ERROR') {
    super(message);
    this.name = 'DrumDeviceError';
    this.code = code;
  }
}

class DrumDevice {
  constructor(midiPort, userInterface) {
    this.midiPort = midiPort;
    this.ui = userInterface;
    this.isConnected = false;
    this.deviceInfo = null;
    
    // Message handling state
    this.messageHandlers = new Map();
    this.pendingResponses = new Map();
    this.customAckQueue = [];
    this.customReplyPromise = null;
  }

  /**
   * Connect to DRUM device
   * @param {string[]} validPortNames - Valid port name patterns
   * @returns {Promise<void>}
   */
  async connect(validPortNames = ['Pico', 'DRUM']) {
    try {
      const { input, output } = await this.midiPort.connect(validPortNames);
      this.input = input;
      this.output = output;
      this.isConnected = true;
      
      // Set up message handler
      this.input.onMessage((deltaTime, message) => {
        this._handleMidiMessage(deltaTime, message);
      });
      
      this.deviceInfo = {
        portName: this.midiPort.getPortName(),
        connected: true
      };
      
      await this.ui.info(`Connected to device: ${this.deviceInfo.portName}`);
    } catch (error) {
      throw new DrumDeviceError(`Failed to connect to device: ${error.message}`, 'CONNECTION_FAILED');
    }
  }

  /**
   * Disconnect from device
   * @returns {Promise<void>}
   */
  async disconnect() {
    if (!this.isConnected) {
      return;
    }
    
    try {
      // Clear any pending responses
      this._clearPendingResponses();
      
      // Remove message handler
      if (this.input) {
        this.input.removeMessageHandler();
      }
      
      // Disconnect MIDI port
      await this.midiPort.disconnect();
      
      this.isConnected = false;
      this.deviceInfo = null;
      this.input = null;
      this.output = null;
    } catch (error) {
      // Log error but don't throw - we want to clean up even if there's an error
      await this.ui.error(`Warning during disconnect: ${error.message}`);
    }
  }

  /**
   * Send SDS message to device
   * @param {number[]} messageData - SDS message data (without SysEx wrapper)
   * @returns {Promise<void>}
   */
  async sendSdsMessage(messageData) {
    this._ensureConnected();
    const message = wrapSdsMessage(messageData);
    await this.output.sendMessage(message);
  }

  /**
   * Send custom protocol message to device
   * @param {number[]} message - Complete custom protocol message
   * @returns {Promise<void>}
   */
  async sendCustomMessage(message) {
    this._ensureConnected();
    await this.output.sendMessage(message);
  }

  /**
   * Wait for SDS response with timeout
   * @param {number} timeout - Timeout in milliseconds
   * @param {number} packetNum - Expected packet number (for validation)
   * @returns {Promise<Object>} Response object
   */
  async waitForSdsResponse(timeout = 2000, packetNum = 0) {
    return new Promise((resolve) => {
      const responseId = `sds_${Date.now()}_${Math.random()}`;
      const timer = setTimeout(() => {
        this.pendingResponses.delete(responseId);
        resolve({ type: 'timeout', packet: packetNum });
      }, timeout);

      this.pendingResponses.set(responseId, {
        resolve,
        timer,
        type: 'sds',
        packetNum
      });
    });
  }

  /**
   * Wait for custom protocol ACK
   * @param {number} timeout - Timeout in milliseconds
   * @returns {Promise<void>}
   */
  async waitForCustomAck(timeout = 2000) {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        const index = this.customAckQueue.findIndex(p => p.timer === timer);
        if (index > -1) {
          this.customAckQueue.splice(index, 1);
        }
        reject(new DrumDeviceError(`Timeout waiting for ACK after ${timeout}ms`, 'ACK_TIMEOUT'));
      }, timeout);

      this.customAckQueue.push({
        resolve,
        reject,
        timer
      });
    });
  }

  /**
   * Wait for custom protocol reply
   * @param {number} timeout - Timeout in milliseconds
   * @returns {Promise<Object>} Reply message
   */
  async waitForCustomReply(timeout = 2000) {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        if (this.customReplyPromise) {
          this.customReplyPromise = null;
        }
        reject(new DrumDeviceError(`Timeout waiting for reply after ${timeout}ms`, 'REPLY_TIMEOUT'));
      }, timeout);

      this.customReplyPromise = {
        resolve: (msg) => {
          clearTimeout(timer);
          resolve(msg);
        },
        reject: (err) => {
          clearTimeout(timer);
          reject(err);
        }
      };
    });
  }

  /**
   * Get device firmware version
   * @returns {Promise<Object>} Version info with major, minor, patch
   */
  async getFirmwareVersion() {
    this._ensureConnected();
    
    try {
      const request = createFirmwareVersionRequest();
      await this.sendCustomMessage(request);
      
      const reply = await this.waitForCustomReply(5000);
      const parsed = parseCustomMessage(reply.raw);
      const version = parseFirmwareVersionResponse(parsed);
      
      if (!version) {
        throw new DrumDeviceError('Invalid firmware version response', 'INVALID_RESPONSE');
      }
      
      return version;
    } catch (error) {
      if (error instanceof DrumDeviceError) {
        throw error;
      }
      throw new DrumDeviceError(`Failed to get firmware version: ${error.message}`, 'VERSION_REQUEST_FAILED');
    }
  }

  /**
   * Format device filesystem
   * @returns {Promise<void>}
   */
  async formatFilesystem() {
    this._ensureConnected();
    
    const confirmed = await this.ui.confirm(
      '⚠️  WARNING: This will erase ALL files on the device filesystem!\nAre you sure you want to format the filesystem?',
      false
    );
    
    if (!confirmed) {
      await this.ui.info('Format cancelled.');
      return;
    }
    
    try {
      await this.ui.info('Sending command to format filesystem...');
      const command = createFormatFilesystemCommand();
      await this.sendCustomMessage(command);
      await this.waitForCustomAck(10000); // Longer timeout for format
      await this.ui.complete('Successfully sent format command. The device will now re-initialize its filesystem.');
    } catch (error) {
      if (error instanceof DrumDeviceError) {
        throw error;
      }
      throw new DrumDeviceError(`Failed to format filesystem: ${error.message}`, 'FORMAT_FAILED');
    }
  }

  /**
   * Reboot device to bootloader mode
   * @returns {Promise<void>}
   */
  async rebootToBootloader() {
    this._ensureConnected();
    
    const confirmed = await this.ui.confirm(
      '⚠️  WARNING: This will reboot the device into bootloader mode!\nThe device will disconnect and enter firmware update mode.\nAre you sure you want to reboot to bootloader?',
      false
    );
    
    if (!confirmed) {
      await this.ui.info('Reboot cancelled.');
      return;
    }
    
    try {
      await this.ui.info('Sending command to reboot to bootloader...');
      const command = createRebootBootloaderCommand();
      await this.sendCustomMessage(command);
      // Note: Device will reboot immediately, so we don't wait for ACK
      await this.ui.complete('Reboot command sent. Device should now enter bootloader mode.');
      
      // Device will disconnect, so mark as disconnected
      this.isConnected = false;
    } catch (error) {
      throw new DrumDeviceError(`Failed to reboot to bootloader: ${error.message}`, 'REBOOT_FAILED');
    }
  }

  /**
   * Register a custom message handler
   * @param {string} handlerName - Name of the handler
   * @param {function} handler - Message handler function
   */
  registerMessageHandler(handlerName, handler) {
    this.messageHandlers.set(handlerName, handler);
  }

  /**
   * Unregister a custom message handler
   * @param {string} handlerName - Name of the handler
   */
  unregisterMessageHandler(handlerName) {
    this.messageHandlers.delete(handlerName);
  }

  /**
   * Get device connection status
   * @returns {boolean} True if connected
   */
  getConnectionStatus() {
    return this.isConnected && this.midiPort.isConnected();
  }

  /**
   * Get device information
   * @returns {Object|null} Device info or null if not connected
   */
  getDeviceInfo() {
    return this.deviceInfo;
  }

  // Private methods

  /**
   * Handle incoming MIDI messages
   * @private
   */
  _handleMidiMessage(deltaTime, message) {
    // Try SDS message handling first
    const sdsResponse = parseSdsResponse(message);
    if (sdsResponse) {
      this._handleSdsResponse(sdsResponse);
      return;
    }
    
    // Try custom protocol message handling
    const customMessage = parseCustomMessage(message);
    if (customMessage) {
      this._handleCustomMessage(customMessage);
      return;
    }
    
    // Call registered custom handlers
    for (const [name, handler] of this.messageHandlers) {
      try {
        handler(deltaTime, message);
      } catch (error) {
        console.warn(`Message handler '${name}' threw error:`, error.message);
      }
    }
  }

  /**
   * Handle SDS response messages
   * @private
   */
  _handleSdsResponse(response) {
    // Find pending SDS response
    for (const [id, pending] of this.pendingResponses) {
      if (pending.type === 'sds') {
        clearTimeout(pending.timer);
        this.pendingResponses.delete(id);
        pending.resolve(response);
        return;
      }
    }
  }

  /**
   * Handle custom protocol messages
   * @private
   */
  _handleCustomMessage(message) {
    if (isAckResponse(message)) {
      const ackResolver = this.customAckQueue.shift();
      if (ackResolver) {
        clearTimeout(ackResolver.timer);
        ackResolver.resolve();
      }
    } else if (isNackResponse(message)) {
      const ackResolver = this.customAckQueue.shift();
      if (ackResolver) {
        clearTimeout(ackResolver.timer);
        ackResolver.reject(new DrumDeviceError('Received NACK from device', 'DEVICE_NACK'));
      }
    } else {
      // Handle other custom messages
      if (this.customReplyPromise) {
        this.customReplyPromise.resolve({ raw: message.raw, parsed: message });
        this.customReplyPromise = null;
      }
    }
  }

  /**
   * Ensure device is connected
   * @private
   */
  _ensureConnected() {
    if (!this.isConnected) {
      throw new DrumDeviceError('Device not connected', 'NOT_CONNECTED');
    }
  }

  /**
   * Clear all pending responses
   * @private
   */
  _clearPendingResponses() {
    // Clear SDS responses
    for (const [id, pending] of this.pendingResponses) {
      clearTimeout(pending.timer);
    }
    this.pendingResponses.clear();
    
    // Clear custom ACK queue
    for (const ackResolver of this.customAckQueue) {
      clearTimeout(ackResolver.timer);
      ackResolver.reject(new DrumDeviceError('Device disconnected', 'DISCONNECTED'));
    }
    this.customAckQueue = [];
    
    // Clear custom reply promise
    if (this.customReplyPromise) {
      this.customReplyPromise.reject(new DrumDeviceError('Device disconnected', 'DISCONNECTED'));
      this.customReplyPromise = null;
    }
  }
}

module.exports = {
  DrumDevice,
  DrumDeviceError
};