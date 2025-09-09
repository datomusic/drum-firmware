/**
 * Node.js User Interface Implementation
 * 
 * Implementation of user interface using Node.js console and readline.
 */

const readline = require('readline');
const { IUserInterface } = require('../interfaces/user_interface');

class NodeUserInterface extends IUserInterface {
  constructor() {
    super();
    this.lastProgressLine = '';
  }

  async info(message) {
    // Clear any existing progress line
    if (this.lastProgressLine) {
      this._clearLine();
      this.lastProgressLine = '';
    }
    console.log(message);
  }

  async error(message) {
    // Clear any existing progress line
    if (this.lastProgressLine) {
      this._clearLine();
      this.lastProgressLine = '';
    }
    console.error(message);
  }

  async warn(message) {
    // Clear any existing progress line
    if (this.lastProgressLine) {
      this._clearLine();
      this.lastProgressLine = '';
    }
    console.warn(message);
  }

  async confirm(message, defaultValue = false) {
    return new Promise((resolve) => {
      const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout
      });
      
      const defaultText = defaultValue ? ' (Y/n)' : ' (y/N)';
      rl.question(`${message}${defaultText}: `, (answer) => {
        rl.close();
        
        if (answer.trim() === '') {
          resolve(defaultValue);
          return;
        }
        
        const confirmed = answer.toLowerCase() === 'y' || answer.toLowerCase() === 'yes';
        resolve(confirmed);
      });
    });
  }

  async updateProgress(progress) {
    const { percentage, message, details } = progress;
    
    // Only update if we have meaningful progress information
    if (message) {
      if (process.stdout.clearLine && process.stdout.cursorTo) {
        // Clear the previous progress line
        if (this.lastProgressLine) {
          process.stdout.clearLine(0);
          process.stdout.cursorTo(0);
        }
        
        // Write new progress line
        process.stdout.write(message);
        this.lastProgressLine = message;
      } else {
        // Fallback for environments without clearLine
        console.log(message);
      }
    }
  }

  async clearProgress() {
    if (this.lastProgressLine && process.stdout.clearLine && process.stdout.cursorTo) {
      process.stdout.clearLine(0);
      process.stdout.cursorTo(0);
      this.lastProgressLine = '';
    }
  }

  async complete(message, details = null) {
    // Clear any existing progress line first
    await this.clearProgress();
    
    console.log(message);
    
    if (details && typeof details === 'object') {
      // Pretty print details if provided
      const detailsStr = JSON.stringify(details, null, 2);
      console.log(detailsStr);
    }
  }

  // Additional Node.js specific methods

  /**
   * Display verbose information (only shown when verbose mode enabled)
   * @param {string} message - Verbose message
   * @param {boolean} verbose - Whether verbose mode is enabled
   */
  async verbose(message, verbose = false) {
    if (verbose) {
      await this.info(message);
    }
  }

  /**
   * Log raw output without any formatting
   * @param {string} message - Raw message
   */
  async raw(message) {
    process.stdout.write(message);
  }

  /**
   * Start a progress spinner or indicator
   * @param {string} message - Progress message
   */
  startProgress(message) {
    process.stdout.write(`${message}...`);
  }

  /**
   * Stop progress indicator and optionally show result
   * @param {string} result - Result message (e.g., 'done', 'failed')
   */
  stopProgress(result = 'done') {
    console.log(` ${result}`);
  }

  // Private methods

  _clearLine() {
    if (process.stdout.clearLine && process.stdout.cursorTo) {
      process.stdout.clearLine(0);
      process.stdout.cursorTo(0);
    }
  }
}

module.exports = {
  NodeUserInterface
};