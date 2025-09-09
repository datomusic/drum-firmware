/**
 * File System Interface
 * 
 * Defines the contract for file system operations across platforms.
 * Implementations should handle platform-specific file I/O.
 */

class IFileSystem {
  /**
   * Check if file exists
   * @param {string} filePath - Path to file
   * @returns {Promise<boolean>}
   */
  async exists(filePath) {
    throw new Error('exists() must be implemented by subclass');
  }

  /**
   * Read file as buffer
   * @param {string} filePath - Path to file
   * @returns {Promise<Buffer|ArrayBuffer>}
   * @throws {Error} If file not found or read error
   */
  async readFile(filePath) {
    throw new Error('readFile() must be implemented by subclass');
  }

  /**
   * Get file size in bytes
   * @param {string} filePath - Path to file
   * @returns {Promise<number>}
   * @throws {Error} If file not found
   */
  async getFileSize(filePath) {
    throw new Error('getFileSize() must be implemented by subclass');
  }

  /**
   * Get file extension from path
   * @param {string} filePath - Path to file
   * @returns {string} File extension (e.g., '.wav', '.pcm')
   */
  getFileExtension(filePath) {
    const lastDot = filePath.lastIndexOf('.');
    return lastDot === -1 ? '' : filePath.substring(lastDot).toLowerCase();
  }

  /**
   * Get filename from path
   * @param {string} filePath - Path to file
   * @returns {string} Filename without path
   */
  getFileName(filePath) {
    const lastSlash = Math.max(filePath.lastIndexOf('/'), filePath.lastIndexOf('\\'));
    return lastSlash === -1 ? filePath : filePath.substring(lastSlash + 1);
  }
}

module.exports = {
  IFileSystem
};