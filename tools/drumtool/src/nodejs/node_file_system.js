/**
 * Node.js File System Implementation
 * 
 * Implementation of file system interface using Node.js fs module.
 */

const fs = require('fs').promises;
const fsSync = require('fs');
const path = require('path');
const { IFileSystem } = require('../interfaces/file_system');

class NodeFileSystem extends IFileSystem {
  constructor() {
    super();
  }

  async exists(filePath) {
    try {
      await fs.access(filePath);
      return true;
    } catch (error) {
      return false;
    }
  }

  async readFile(filePath) {
    try {
      return await fs.readFile(filePath);
    } catch (error) {
      throw new Error(`Failed to read file '${filePath}': ${error.message}`);
    }
  }

  async getFileSize(filePath) {
    try {
      const stats = await fs.stat(filePath);
      return stats.size;
    } catch (error) {
      throw new Error(`Failed to get file size for '${filePath}': ${error.message}`);
    }
  }

  getFileExtension(filePath) {
    return path.extname(filePath).toLowerCase();
  }

  getFileName(filePath) {
    return path.basename(filePath);
  }

  getDirectory(filePath) {
    return path.dirname(filePath);
  }

  joinPath(...parts) {
    return path.join(...parts);
  }

  // Synchronous methods for compatibility
  existsSync(filePath) {
    return fsSync.existsSync(filePath);
  }

  readFileSync(filePath) {
    try {
      return fsSync.readFileSync(filePath);
    } catch (error) {
      throw new Error(`Failed to read file '${filePath}': ${error.message}`);
    }
  }
}

module.exports = {
  NodeFileSystem
};