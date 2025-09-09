/**
 * FileProcessor - Audio File Processing
 * 
 * This class handles loading and processing audio files for DRUM device transfer.
 * It supports WAV files (with automatic conversion) and raw PCM data.
 */

class FileProcessorError extends Error {
  constructor(message, code = 'FILE_ERROR') {
    super(message);
    this.name = 'FileProcessorError';
    this.code = code;
  }
}

class FileProcessor {
  constructor(fileSystem, userInterface) {
    this.fs = fileSystem;
    this.ui = userInterface;
    
    // Try to load WaveFile library if available (Node.js environment)
    this.WaveFile = null;
    try {
      this.WaveFile = require('wavefile').WaveFile;
    } catch (e) {
      // WaveFile not available - that's okay, we'll handle raw PCM only
    }
  }

  /**
   * Process audio file for device transfer
   * @param {string} filePath - Path to audio file
   * @param {number} sampleRate - Target sample rate (used for PCM files)
   * @param {boolean} verbose - Enable verbose logging
   * @returns {Promise<Object>} Processed audio data
   */
  async processAudioFile(filePath, sampleRate = 44100, verbose = false) {
    // Check if file exists
    const exists = await this.fs.exists(filePath);
    if (!exists) {
      throw new FileProcessorError(`Source file not found at '${filePath}'`, 'FILE_NOT_FOUND');
    }

    const fileExtension = this.fs.getFileExtension(filePath);
    const fileName = this.fs.getFileName(filePath);

    if (verbose) {
      await this.ui.info(`Processing file: ${fileName} (${fileExtension})`);
    }

    // Read file data
    const fileBuffer = await this.fs.readFile(filePath);

    let processedData;
    
    if (fileExtension === '.wav' && this.WaveFile) {
      processedData = await this._processWavFile(fileBuffer, sampleRate, verbose);
    } else if (fileExtension === '.pcm' || !this.WaveFile) {
      processedData = await this._processRawPcmFile(fileBuffer, sampleRate, verbose);
    } else {
      throw new FileProcessorError(
        `Unsupported file format '${fileExtension}'. Supported formats: .wav, .pcm`, 
        'UNSUPPORTED_FORMAT'
      );
    }

    if (verbose) {
      await this.ui.info(`Processed: ${processedData.pcmData.length} bytes, ${processedData.sampleRate}Hz, 16-bit mono`);
    }

    return {
      ...processedData,
      originalPath: filePath,
      fileName,
      fileExtension
    };
  }

  /**
   * Validate slot number
   * @param {number} slot - Slot number to validate
   * @returns {boolean} True if valid
   */
  validateSlot(slot) {
    return Number.isInteger(slot) && slot >= 0 && slot <= 127;
  }

  /**
   * Generate target filename for device
   * @param {number} slot - Target slot number
   * @returns {string} Device filename
   */
  generateTargetFilename(slot) {
    if (!this.validateSlot(slot)) {
      throw new FileProcessorError(`Invalid slot number: ${slot}. Must be 0-127.`, 'INVALID_SLOT');
    }
    return `${slot.toString().padStart(2, '0')}.pcm`;
  }

  /**
   * Parse file:slot argument format
   * @param {string} arg - Argument in format "file:slot"
   * @returns {Object} Parsed file path and slot
   */
  parseFileSlotArg(arg) {
    const colonIndex = arg.indexOf(':');
    if (colonIndex === -1) {
      throw new FileProcessorError(`Invalid format '${arg}'. Expected 'file:slot' format.`, 'INVALID_FORMAT');
    }
    
    const filePath = arg.substring(0, colonIndex);
    const slotStr = arg.substring(colonIndex + 1);
    const slot = parseInt(slotStr, 10);
    
    if (isNaN(slot) || !this.validateSlot(slot)) {
      throw new FileProcessorError(`Invalid slot number '${slotStr}'. Must be 0-127.`, 'INVALID_SLOT');
    }
    
    return { filePath, slot };
  }

  /**
   * Check if argument is a sample rate
   * @param {string} arg - Argument to check
   * @returns {boolean} True if argument looks like a sample rate
   */
  isSampleRate(arg) {
    const rateMatch = arg.match(/^(\d+)$/);
    return !!rateMatch && !arg.includes(':') && parseInt(arg, 10) >= 1000;
  }

  /**
   * Parse command arguments for file transfers
   * @param {string[]} args - Command arguments
   * @returns {Object} Parsed transfer configuration
   */
  parseTransferArgs(args) {
    let sampleRate = 44100;
    const fileArgs = [];
    
    // Extract sample rate and filter file arguments
    for (const arg of args) {
      if (arg === '--verbose' || arg === '-v') {
        continue; // Skip flags
      }
      
      if (this.isSampleRate(arg)) {
        sampleRate = parseInt(arg, 10);
        continue;
      }
      
      fileArgs.push(arg);
    }
    
    if (fileArgs.length === 0) {
      throw new FileProcessorError('No file arguments provided.', 'NO_FILES');
    }
    
    // Detect mode: check if ANY argument contains ':'
    const hasSlotSyntax = fileArgs.some(arg => arg.includes(':'));
    
    let transfers = [];
    
    if (hasSlotSyntax) {
      // Mode A: ALL must use filename:slot format
      transfers = this._parseExplicitSlots(fileArgs, sampleRate);
    } else {
      // Mode B: ALL are filenames, auto-increment from 0
      transfers = this._parseAutoIncrement(fileArgs, sampleRate);
    }
    
    return {
      transfers,
      sampleRate,
      mode: hasSlotSyntax ? 'explicit' : 'auto'
    };
  }

  // Private methods

  /**
   * Process WAV file
   * @private
   */
  async _processWavFile(fileBuffer, sampleRate, verbose) {
    try {
      const wav = new this.WaveFile(fileBuffer);
      
      // Convert to 16-bit if needed
      if (wav.fmt.bitsPerSample !== 16) {
        if (verbose) {
          await this.ui.info(`Converting from ${wav.fmt.bitsPerSample}-bit to 16-bit...`);
        }
        wav.toBitDepth('16');
      }
      
      let finalSampleRate = wav.fmt.sampleRate;
      if (verbose && sampleRate !== 44100 && sampleRate !== finalSampleRate) {
        await this.ui.info(`Note: Using sample rate from WAV file (${finalSampleRate}Hz) instead of command-line argument.`);
      }
      
      // Get samples and convert to mono if needed
      let samples = wav.getSamples(false); // Get as array of samples
      if (wav.fmt.numChannels !== 1) {
        if (verbose) {
          await this.ui.info('Converting stereo to mono...');
        }
        samples = this._convertToMono(samples, wav.fmt.numChannels);
      }
      
      // Convert samples array back to buffer (16-bit little endian)
      const pcmData = this._samplesToBuffer(samples);
      
      return {
        pcmData,
        sampleRate: finalSampleRate,
        bitDepth: 16,
        channels: 1,
        originalFormat: {
          sampleRate: wav.fmt.sampleRate,
          bitDepth: wav.fmt.bitsPerSample,
          channels: wav.fmt.numChannels
        }
      };
      
    } catch (error) {
      throw new FileProcessorError(`Failed to process WAV file: ${error.message}`, 'WAV_PROCESSING_FAILED');
    }
  }

  /**
   * Process raw PCM file
   * @private
   */
  async _processRawPcmFile(fileBuffer, sampleRate, verbose) {
    if (verbose) {
      await this.ui.info(`Treating as raw 16-bit PCM data (${sampleRate}Hz)`);
    }
    
    // Ensure we have a Buffer-like object for consistent interface
    let pcmData;
    if (fileBuffer.constructor === ArrayBuffer) {
      // Convert ArrayBuffer to a buffer-like object with readInt16LE method
      pcmData = new DataView(fileBuffer);
      pcmData.readInt16LE = function(offset) {
        return this.getInt16(offset, true); // true = little endian
      };
      pcmData.length = fileBuffer.byteLength;
    } else {
      pcmData = fileBuffer;
    }
    
    return {
      pcmData,
      sampleRate,
      bitDepth: 16,
      channels: 1,
      originalFormat: {
        sampleRate,
        bitDepth: 16,
        channels: 1
      }
    };
  }

  /**
   * Convert stereo/multi-channel samples to mono
   * @private
   */
  _convertToMono(samples, numChannels) {
    const monoSamples = [];
    for (let i = 0; i < samples.length; i += numChannels) {
      let sum = 0;
      for (let ch = 0; ch < numChannels; ch++) {
        sum += samples[i + ch] || 0;
      }
      monoSamples.push(sum / numChannels);
    }
    return monoSamples;
  }

  /**
   * Convert samples array to 16-bit PCM buffer
   * @private
   */
  _samplesToBuffer(samples) {
    // Create buffer - try Node.js Buffer first, fall back to ArrayBuffer
    let pcmData;
    let writeInt16LE;
    
    if (typeof Buffer !== 'undefined') {
      // Node.js environment
      pcmData = Buffer.alloc(samples.length * 2);
      writeInt16LE = (value, offset) => pcmData.writeInt16LE(value, offset);
    } else {
      // Browser environment
      const arrayBuffer = new ArrayBuffer(samples.length * 2);
      const dataView = new DataView(arrayBuffer);
      pcmData = arrayBuffer;
      pcmData.length = arrayBuffer.byteLength;
      pcmData.readInt16LE = (offset) => dataView.getInt16(offset, true);
      writeInt16LE = (value, offset) => dataView.setInt16(offset, value, true);
    }
    
    for (let i = 0; i < samples.length; i++) {
      const sample = Math.max(-32768, Math.min(32767, Math.round(samples[i])));
      writeInt16LE(sample, i * 2);
    }
    
    return pcmData;
  }

  /**
   * Parse explicit slot format
   * @private
   */
  _parseExplicitSlots(fileArgs, sampleRate) {
    const transfers = [];
    
    for (const arg of fileArgs) {
      const colonIndex = arg.indexOf(':');
      if (colonIndex === -1) {
        throw new FileProcessorError(
          `Mixed formats not allowed. Use either 'filename:slot' for all files or just 'filename' for all files.`,
          'MIXED_FORMATS'
        );
      }
      
      const { filePath, slot } = this.parseFileSlotArg(arg);
      transfers.push({ filePath, slot, sampleRate });
    }
    
    return transfers;
  }

  /**
   * Parse auto-increment format
   * @private
   */
  _parseAutoIncrement(fileArgs, sampleRate) {
    const transfers = [];
    
    fileArgs.forEach((filePath, index) => {
      if (index > 127) {
        throw new FileProcessorError('Too many files. Maximum 128 slots (0-127).', 'TOO_MANY_FILES');
      }
      transfers.push({ filePath, slot: index, sampleRate });
    });
    
    return transfers;
  }
}

module.exports = {
  FileProcessor,
  FileProcessorError
};