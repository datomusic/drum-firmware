/**
 * SampleTransfer - Orchestrates Sample Transfer to DRUM Device
 * 
 * This class handles the complete workflow of transferring audio samples
 * to DATO DRUM devices using the SDS protocol.
 */

const { 
  createDumpHeader, 
  createDataPacket,
  createCancelMessage,
  RESPONSE_TYPES 
} = require('../protocol/sds_protocol');

class SampleTransferError extends Error {
  constructor(message, code = 'TRANSFER_ERROR') {
    super(message);
    this.name = 'SampleTransferError';
    this.code = code;
  }
}

class TransferProgress {
  constructor() {
    this.reset();
  }

  reset() {
    this.totalPackets = 0;
    this.completedPackets = 0;
    this.percentage = 0;
    this.status = 'idle'; // idle, header, transferring, complete, error
    this.deviceStatus = 'unknown'; // ok, slow, error, unknown
    this.startTime = null;
    this.endTime = null;
    this.currentSample = null;
  }

  update(completedPackets, totalPackets, status = null, deviceStatus = null) {
    this.completedPackets = completedPackets;
    this.totalPackets = totalPackets;
    this.percentage = totalPackets > 0 ? (completedPackets / totalPackets) * 100 : 0;
    
    if (status !== null) {
      this.status = status;
    }
    if (deviceStatus !== null) {
      this.deviceStatus = deviceStatus;
    }
  }

  start(sampleInfo) {
    this.reset();
    this.startTime = Date.now();
    this.status = 'header';
    this.currentSample = sampleInfo;
  }

  complete() {
    this.endTime = Date.now();
    this.status = 'complete';
    this.percentage = 100;
  }

  getDurationMs() {
    if (!this.startTime) return 0;
    const endTime = this.endTime || Date.now();
    return endTime - this.startTime;
  }

  getDurationSeconds() {
    return (this.getDurationMs() / 1000).toFixed(1);
  }
}

class SampleTransfer {
  constructor(drumDevice, userInterface) {
    this.device = drumDevice;
    this.ui = userInterface;
    this.progress = new TransferProgress();
    this.isActive = false;
    
    // Transfer configuration
    this.config = {
      headerTimeout: 2000,    // 2 seconds for header ACK
      packetTimeout: 20,      // 20ms per packet as per SDS spec
      maxWaitTime: 30000,     // 30 seconds max for WAIT responses
      progressUpdateThreshold: 1 // Update progress every 1%
    };
  }

  /**
   * Transfer a single sample to the device
   * @param {Object} sampleData - Processed sample data from FileProcessor
   * @param {number} slot - Target slot number
   * @param {boolean} verbose - Enable verbose logging
   * @returns {Promise<boolean>} True if transfer successful
   */
  async transferSample(sampleData, slot, verbose = false) {
    const { pcmData, sampleRate, fileName } = sampleData;
    const targetFilename = `${slot.toString().padStart(2, '0')}.pcm`;

    if (verbose) {
      await this.ui.info(`\n=== SDS Transfer: ${fileName} → /${targetFilename} (slot ${slot}) ===`);
    }

    try {
      this.isActive = true;
      this.progress.start({ 
        fileName, 
        slot, 
        targetFilename,
        sampleRate,
        dataSize: pcmData.length
      });

      // Calculate transfer parameters
      const totalPackets = Math.ceil(pcmData.length / 80); // 40 samples * 2 bytes per packet
      this.progress.update(0, totalPackets);

      if (verbose) {
        await this.ui.info(`Transfer will require ${totalPackets} data packets`);
      }

      // Step 1: Send Dump Header
      const success = await this._transferWithHeader(pcmData, slot, sampleRate, verbose);
      
      if (success) {
        this.progress.complete();
        const duration = this.progress.getDurationSeconds();
        await this.ui.complete(`Transfer complete: ${this.progress.completedPackets} packets sent in ${duration}s`);
      }
      
      return success;
      
    } catch (error) {
      this.progress.status = 'error';
      throw error;
    } finally {
      this.isActive = false;
    }
  }

  /**
   * Transfer multiple samples in batch
   * @param {Array} transfers - Array of transfer configurations
   * @param {boolean} verbose - Enable verbose logging
   * @returns {Promise<boolean>} True if all transfers successful
   */
  async transferMultipleSamples(transfers, verbose = false) {
    await this.ui.info(`\n=== Batch SDS Transfer: ${transfers.length} samples ===`);
    
    const results = [];
    let successCount = 0;
    
    for (let i = 0; i < transfers.length; i++) {
      const transfer = transfers[i];
      await this.ui.info(`\n[${i + 1}/${transfers.length}] Processing ${transfer.fileName || transfer.filePath}`);
      
      try {
        const success = await this.transferSample(transfer.sampleData, transfer.slot, verbose);
        results.push({ ...transfer, success, error: null });
        if (success) {
          successCount++;
        }
      } catch (error) {
        await this.ui.error(`Failed: ${error.message}`);
        results.push({ ...transfer, success: false, error: error.message });
      }
      
      // Brief pause between transfers to avoid overwhelming the device
      if (i < transfers.length - 1) {
        await new Promise(resolve => setTimeout(resolve, 100));
      }
    }
    
    // Summary
    await this.ui.info(`\n=== Transfer Summary ===`);
    await this.ui.info(`Successful: ${successCount}/${transfers.length}`);
    
    if (successCount < transfers.length) {
      await this.ui.info('\nFailed transfers:');
      results.filter(r => !r.success).forEach(async r => {
        await this.ui.error(`  ${r.fileName || r.filePath} → slot ${r.slot}: ${r.error}`);
      });
    }
    
    return successCount === transfers.length;
  }

  /**
   * Cancel active transfer
   * @returns {Promise<void>}
   */
  async cancelTransfer() {
    if (!this.isActive || !this.progress.currentSample) {
      return;
    }

    try {
      const { slot } = this.progress.currentSample;
      const cancelMessage = createCancelMessage(slot);
      await this.device.sendSdsMessage(cancelMessage);
      await this.ui.info(`Cancel command sent for sample ${slot}.`);
    } catch (error) {
      await this.ui.error(`Error sending cancel command: ${error.message}`);
    }

    this.progress.status = 'cancelled';
    this.isActive = false;
  }

  /**
   * Get current transfer progress
   * @returns {Object} Progress information
   */
  getProgress() {
    return {
      percentage: this.progress.percentage,
      completedPackets: this.progress.completedPackets,
      totalPackets: this.progress.totalPackets,
      status: this.progress.status,
      deviceStatus: this.progress.deviceStatus,
      isActive: this.isActive,
      currentSample: this.progress.currentSample,
      durationMs: this.progress.getDurationMs()
    };
  }

  // Private methods

  /**
   * Perform transfer with header negotiation
   * @private
   */
  async _transferWithHeader(pcmData, sampleNumber, sampleRate, verbose) {
    // Step 1: Send Dump Header
    const header = createDumpHeader(sampleNumber, 16, sampleRate, pcmData.length);
    
    if (verbose) {
      await this.ui.info("\n1. Sending Dump Header...");
    }
    
    await this.device.sendSdsMessage(header);
    
    // Wait for header response
    const headerResponse = await this.device.waitForSdsResponse(this.config.headerTimeout);
    
    if (headerResponse.type === RESPONSE_TYPES.NAK) {
      if (verbose) {
        await this.ui.info("Header NAK received, retrying...");
      }
      
      // Retry header once
      await this.device.sendSdsMessage(header);
      const retryResponse = await this.device.waitForSdsResponse(this.config.headerTimeout);
      
      if (retryResponse.type !== RESPONSE_TYPES.ACK && retryResponse.type !== RESPONSE_TYPES.TIMEOUT) {
        throw new SampleTransferError("Header rejected twice", "HEADER_REJECTED");
      }
    }
    
    const handshaking = headerResponse.type === RESPONSE_TYPES.ACK;
    
    if (verbose) {
      const mode = handshaking ? 'handshaking mode' : 'non-handshaking mode';
      await this.ui.info(`Header ${headerResponse.type} - ${mode}`);
      await this.ui.info("\n2. Sending data packets...");
    }
    
    this.progress.update(0, Math.ceil(pcmData.length / 80), 'transferring');
    
    // Step 2: Send Data Packets
    return await this._sendDataPackets(pcmData, handshaking, verbose);
  }

  /**
   * Send data packets with progress tracking
   * @private
   */
  async _sendDataPackets(pcmData, handshaking, verbose) {
    let packetNum = 0;
    let offset = 0;
    let successfulPackets = 0;
    
    const totalPackets = Math.ceil(pcmData.length / 80);
    const showProgress = totalPackets > 4;
    let lastProgressUpdate = 0;
    
    while (offset < pcmData.length) {
      const packet = createDataPacket(packetNum & 0x7F, pcmData, offset);
      
      if (verbose && packetNum < 5) {
        await this.ui.info(`Packet ${packetNum} size: ${packet.length}, checksum: 0x${packet[packet.length-1].toString(16)}`);
      }
      
      await this.device.sendSdsMessage(packet);
      
      if (handshaking) {
        const result = await this._handleHandshakingResponse(packetNum & 0x7F, verbose, showProgress);
        
        if (result === 'success') {
          this.progress.deviceStatus = 'ok';
          successfulPackets++;
          offset += 80;
          packetNum++;
        } else if (result === 'retry') {
          continue; // Retry same packet
        } else if (result === 'switch_mode') {
          // Switch to non-handshaking mode
          handshaking = false;
          successfulPackets++;
          offset += 80;
          packetNum++;
        } else if (result === 'error') {
          throw new SampleTransferError('Device cancelled transfer - storage may be full', 'TRANSFER_CANCELLED');
        }
      } else {
        // Non-handshaking mode - advance immediately
        successfulPackets++;
        offset += 80;
        packetNum++;
      }
      
      // Update progress
      this.progress.update(successfulPackets, totalPackets);
      
      if (showProgress) {
        await this._updateProgressDisplay(successfulPackets, totalPackets, lastProgressUpdate);
        lastProgressUpdate = Math.round(this.progress.percentage);
      }
    }
    
    return true;
  }

  /**
   * Handle response in handshaking mode
   * @private
   */
  async _handleHandshakingResponse(packetNum, verbose, showProgress) {
    const response = await this.device.waitForSdsResponse(this.config.packetTimeout, packetNum);
    
    switch (response.type) {
      case RESPONSE_TYPES.ACK:
        return 'success';
        
      case RESPONSE_TYPES.NAK:
        if (verbose) {
          const message = `Packet ${packetNum} NAK - retrying`;
          if (showProgress) {
            await this.ui.info(`\n${message}`);
          } else {
            await this.ui.info(message);
          }
        }
        return 'retry';
        
      case RESPONSE_TYPES.WAIT:
        if (verbose) {
          await this.ui.info(`\nPacket ${packetNum} WAIT - device is busy, waiting for final response...`);
        }
        this.progress.deviceStatus = 'slow';
        
        // Wait for final response after WAIT
        const finalResponse = await this.device.waitForSdsResponse(this.config.maxWaitTime, packetNum);
        
        if (finalResponse.type === RESPONSE_TYPES.ACK) {
          return 'success';
        } else if (finalResponse.type === RESPONSE_TYPES.NAK) {
          return 'retry';
        } else if (finalResponse.type === RESPONSE_TYPES.CANCEL) {
          return 'error';
        } else {
          // Even WAIT can timeout - switch to non-handshaking
          if (verbose) {
            await this.ui.info(`Final response timeout after WAIT - assuming non-handshaking`);
          }
          return 'switch_mode';
        }
        
      case RESPONSE_TYPES.CANCEL:
        return 'error';
        
      default: // TIMEOUT
        if (verbose) {
          const message = `Packet ${packetNum} timeout - continuing without handshaking`;
          if (showProgress) {
            await this.ui.info(`\n${message}`);
          } else {
            await this.ui.info(message);
          }
        }
        return 'switch_mode';
    }
  }

  /**
   * Update progress display
   * @private
   */
  async _updateProgressDisplay(completedPackets, totalPackets, lastUpdate) {
    const progressPercent = Math.round(this.progress.percentage);
    
    // Only update every 1% or every 50 packets to reduce flicker
    if (progressPercent > lastUpdate || completedPackets % 50 === 0) {
      const progressInfo = {
        percentage: progressPercent,
        message: this._getProgressMessage(completedPackets, totalPackets),
        details: {
          completed: completedPackets,
          total: totalPackets,
          deviceStatus: this.progress.deviceStatus
        }
      };
      
      await this.ui.updateProgress(progressInfo);
    }
  }

  /**
   * Generate progress message
   * @private
   */
  _getProgressMessage(completedPackets, totalPackets) {
    const statusText = this.progress.deviceStatus === 'slow' ? '[BUSY]' : 
                      this.progress.deviceStatus === 'error' ? '[ERROR]' : 
                      this.progress.deviceStatus === 'ok' ? '[OK]' : '[??]';
    
    const percentage = Math.round(this.progress.percentage);
    const barLength = 40;
    const filledLength = Math.round(barLength * (percentage / 100));
    const bar = '█'.repeat(filledLength) + '░'.repeat(barLength - filledLength);
    
    return `${statusText} Progress: ${percentage}% |${bar}| ${completedPackets}/${totalPackets} packets`;
  }
}

module.exports = {
  SampleTransfer,
  SampleTransferError,
  TransferProgress
};