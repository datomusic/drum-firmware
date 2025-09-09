/**
 * User Interface Interface
 * 
 * Defines the contract for user interaction across platforms.
 * Implementations should handle platform-specific UI operations.
 */

class IUserInterface {
  /**
   * Display information message to user
   * @param {string} message - Message to display
   * @returns {Promise<void>}
   */
  async info(message) {
    throw new Error('info() must be implemented by subclass');
  }

  /**
   * Display error message to user
   * @param {string} message - Error message to display
   * @returns {Promise<void>}
   */
  async error(message) {
    throw new Error('error() must be implemented by subclass');
  }

  /**
   * Display warning message to user
   * @param {string} message - Warning message to display
   * @returns {Promise<void>}
   */
  async warn(message) {
    throw new Error('warn() must be implemented by subclass');
  }

  /**
   * Ask user for yes/no confirmation
   * @param {string} message - Confirmation prompt message
   * @param {boolean} defaultValue - Default value if user doesn't respond
   * @returns {Promise<boolean>} true if user confirms, false otherwise
   */
  async confirm(message, defaultValue = false) {
    throw new Error('confirm() must be implemented by subclass');
  }

  /**
   * Update progress display
   * @param {Object} progress - Progress information
   * @param {number} progress.percentage - Progress percentage (0-100)
   * @param {string} progress.message - Progress message
   * @param {Object} [progress.details] - Additional progress details
   * @returns {Promise<void>}
   */
  async updateProgress(progress) {
    throw new Error('updateProgress() must be implemented by subclass');
  }

  /**
   * Clear progress display
   * @returns {Promise<void>}
   */
  async clearProgress() {
    throw new Error('clearProgress() must be implemented by subclass');
  }

  /**
   * Display completion message
   * @param {string} message - Completion message
   * @param {Object} [details] - Additional completion details
   * @returns {Promise<void>}
   */
  async complete(message, details = null) {
    throw new Error('complete() must be implemented by subclass');
  }
}

module.exports = {
  IUserInterface
};