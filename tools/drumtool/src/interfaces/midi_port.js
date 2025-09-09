/**
 * MIDI Port Interface
 * 
 * Defines the contract for MIDI communication across platforms.
 * Implementations should handle platform-specific MIDI operations.
 */

class IMidiPort {
  /**
   * Connect to MIDI device
   * @param {string[]} validPortNames - Array of valid port name patterns
   * @returns {Promise<{input: IMidiInput, output: IMidiOutput}>}
   * @throws {Error} If no suitable device found
   */
  async connect(validPortNames) {
    throw new Error('connect() must be implemented by subclass');
  }

  /**
   * Disconnect from MIDI device
   * @returns {Promise<void>}
   */
  async disconnect() {
    throw new Error('disconnect() must be implemented by subclass');
  }

  /**
   * Check if currently connected
   * @returns {boolean}
   */
  isConnected() {
    throw new Error('isConnected() must be implemented by subclass');
  }

  /**
   * Get connected port name
   * @returns {string|null}
   */
  getPortName() {
    throw new Error('getPortName() must be implemented by subclass');
  }
}

class IMidiInput {
  /**
   * Set message handler for incoming MIDI messages
   * @param {function(number, Uint8Array): void} handler - Message handler function
   */
  onMessage(handler) {
    throw new Error('onMessage() must be implemented by subclass');
  }

  /**
   * Remove message handler
   */
  removeMessageHandler() {
    throw new Error('removeMessageHandler() must be implemented by subclass');
  }
}

class IMidiOutput {
  /**
   * Send MIDI message
   * @param {number[]|Uint8Array} message - MIDI message bytes
   * @returns {Promise<void>}
   */
  async sendMessage(message) {
    throw new Error('sendMessage() must be implemented by subclass');
  }
}

module.exports = {
  IMidiPort,
  IMidiInput,
  IMidiOutput
};