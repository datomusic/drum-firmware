/**
 * Drumtool CLI - Command Line Interface
 * 
 * This class handles command-line interface logic, argument parsing,
 * and orchestrates the various components to fulfill user commands.
 */

const { DrumDevice } = require('../core/drum_device');
const { FileProcessor } = require('../core/file_processor');
const { SampleTransfer } = require('../core/sample_transfer');

class DrumtoolCliError extends Error {
  constructor(message, code = 'CLI_ERROR', exitCode = 1) {
    super(message);
    this.name = 'DrumtoolCliError';
    this.code = code;
    this.exitCode = exitCode;
  }
}

class DrumtoolCli {
  constructor(midiPort, fileSystem, userInterface) {
    this.midiPort = midiPort;
    this.fileSystem = fileSystem;
    this.ui = userInterface;
    
    // Core components (initialized on connect)
    this.device = null;
    this.fileProcessor = null;
    this.sampleTransfer = null;
    
    // CLI state
    this.isConnected = false;
    this.verbose = false;
    
    // Setup signal handlers
    this._setupSignalHandlers();
  }

  /**
   * Run the CLI with given arguments
   * @param {string[]} argv - Command line arguments (e.g., process.argv)
   * @returns {Promise<number>} Exit code
   */
  async run(argv) {
    try {
      const { command, args } = this._parseArguments(argv);
      
      // Handle help/usage
      if (!command) {
        this._showUsage();
        return 1;
      }
      
      // Validate command early (before MIDI initialization)
      this._validateCommand(command, args);
      
      // Initialize components
      await this._initializeComponents();
      
      // Execute command
      const success = await this._executeCommand(command, args);
      return success ? 0 : 1;
      
    } catch (error) {
      if (error instanceof DrumtoolCliError) {
        await this.ui.error(`Error: ${error.message}`);
        return error.exitCode;
      } else {
        await this.ui.error(`Unexpected error: ${error.message}`);
        if (this.verbose) {
          console.error(error.stack);
        }
        return 1;
      }
    } finally {
      await this._cleanup();
    }
  }

  // Private methods

  /**
   * Parse command line arguments
   * @private
   */
  _parseArguments(argv) {
    const args = argv.slice(2); // Remove 'node' and script name
    const command = args[0];
    
    // Check for verbose flag
    this.verbose = args.includes('--verbose') || args.includes('-v');
    
    return {
      command,
      args: args.slice(1) // Remove command from args
    };
  }

  /**
   * Validate command and arguments early
   * @private
   */
  _validateCommand(command, args) {
    const validCommands = ['send', 'version', 'format', 'reboot-bootloader'];
    
    if (!validCommands.includes(command)) {
      throw new DrumtoolCliError(
        `Unknown command '${command}'. Use 'send', 'version', 'format', or 'reboot-bootloader'.`,
        'INVALID_COMMAND'
      );
    }

    if (command === 'send') {
      const filteredArgs = args.filter(arg => arg !== '--verbose' && arg !== '-v');
      
      if (filteredArgs.length === 0) {
        throw new DrumtoolCliError(
          "'send' command requires at least one file argument.",
          'MISSING_ARGUMENTS'
        );
      }
      
      // Validate file arguments early using FileProcessor
      try {
        const tempFileProcessor = new FileProcessor(this.fileSystem, this.ui);
        tempFileProcessor.parseTransferArgs(filteredArgs);
      } catch (error) {
        throw new DrumtoolCliError(error.message, 'INVALID_ARGUMENTS');
      }
    }
  }

  /**
   * Initialize core components
   * @private
   */
  async _initializeComponents() {
    await this.ui.info('Initializing MIDI connection...');
    
    // Create core components
    this.device = new DrumDevice(this.midiPort, this.ui);
    this.fileProcessor = new FileProcessor(this.fileSystem, this.ui);
    this.sampleTransfer = new SampleTransfer(this.device, this.ui);
    
    // Connect to device
    try {
      await this.device.connect(['Pico', 'DRUM']);
      this.isConnected = true;
    } catch (error) {
      throw new DrumtoolCliError(
        `Failed to initialize MIDI connection: ${error.message}`,
        'CONNECTION_FAILED'
      );
    }
  }

  /**
   * Execute the specified command
   * @private
   */
  async _executeCommand(command, args) {
    switch (command) {
      case 'send':
        return await this._handleSendCommand(args);
      case 'version':
        return await this._handleVersionCommand();
      case 'format':
        return await this._handleFormatCommand();
      case 'reboot-bootloader':
        return await this._handleRebootCommand();
      default:
        throw new DrumtoolCliError(`Unknown command: ${command}`, 'INVALID_COMMAND');
    }
  }

  /**
   * Handle 'send' command
   * @private
   */
  async _handleSendCommand(args) {
    const filteredArgs = args.filter(arg => arg !== '--verbose' && arg !== '-v');
    
    try {
      const { transfers } = this.fileProcessor.parseTransferArgs(filteredArgs);
      
      // Process all files first
      const processedTransfers = [];
      for (const transfer of transfers) {
        const sampleData = await this.fileProcessor.processAudioFile(
          transfer.filePath,
          transfer.sampleRate,
          this.verbose
        );
        
        processedTransfers.push({
          ...transfer,
          sampleData,
          fileName: sampleData.fileName
        });
      }
      
      // Transfer samples
      if (processedTransfers.length === 1) {
        // Single transfer
        const transfer = processedTransfers[0];
        return await this.sampleTransfer.transferSample(
          transfer.sampleData,
          transfer.slot,
          this.verbose
        );
      } else {
        // Batch transfer
        return await this.sampleTransfer.transferMultipleSamples(
          processedTransfers,
          this.verbose
        );
      }
      
    } catch (error) {
      throw new DrumtoolCliError(`Transfer failed: ${error.message}`, 'TRANSFER_FAILED');
    }
  }

  /**
   * Handle 'version' command
   * @private
   */
  async _handleVersionCommand() {
    try {
      const version = await this.device.getFirmwareVersion();
      await this.ui.info(`Device firmware version: v${version.major}.${version.minor}.${version.patch}`);
      return true;
    } catch (error) {
      throw new DrumtoolCliError(`Failed to get firmware version: ${error.message}`, 'VERSION_FAILED');
    }
  }

  /**
   * Handle 'format' command
   * @private
   */
  async _handleFormatCommand() {
    try {
      await this.device.formatFilesystem();
      return true;
    } catch (error) {
      if (error.message.includes('cancelled')) {
        return true; // User cancelled, not an error
      }
      throw new DrumtoolCliError(`Format failed: ${error.message}`, 'FORMAT_FAILED');
    }
  }

  /**
   * Handle 'reboot-bootloader' command
   * @private
   */
  async _handleRebootCommand() {
    try {
      await this.device.rebootToBootloader();
      return true;
    } catch (error) {
      if (error.message.includes('cancelled')) {
        return true; // User cancelled, not an error
      }
      throw new DrumtoolCliError(`Reboot failed: ${error.message}`, 'REBOOT_FAILED');
    }
  }

  /**
   * Setup signal handlers for graceful shutdown
   * @private
   */
  _setupSignalHandlers() {
    const handleSignal = async (signal) => {
      await this.ui.info(`\n\nReceived ${signal}, shutting down...`);
      
      // Cancel any active transfer
      if (this.sampleTransfer) {
        await this.sampleTransfer.cancelTransfer();
      }
      
      await this._cleanup();
      process.exit(0);
    };

    process.on('SIGINT', handleSignal);
    process.on('SIGTERM', handleSignal);
  }

  /**
   * Cleanup resources
   * @private
   */
  async _cleanup() {
    if (this.isConnected && this.device) {
      try {
        await this.device.disconnect();
      } catch (error) {
        // Log but don't throw during cleanup
        if (this.verbose) {
          console.warn('Warning during cleanup:', error.message);
        }
      }
    }
    this.isConnected = false;
  }

  /**
   * Show usage information
   * @private
   */
  _showUsage() {
    const usage = `Drumtool - DRUM Device Management Tool

Usage:
  drumtool.js <command> [options]

Commands:
  send           - Transfer audio samples (WAV or raw PCM) using SDS protocol
  version        - Get device firmware version
  format         - Format device filesystem
  reboot-bootloader - Reboot device into bootloader mode

Send Arguments:
  file:slot      - Audio file path and target slot (0-127)
  file           - Audio file path (auto-assigns slots 0,1,2... in order)
  sample_rate    - Sample rate in Hz for raw PCM files (default: 44100).
                   For WAV files, the sample rate is detected automatically.
  --verbose, -v  - Show detailed transfer information

Examples:
  # Explicit slot assignment (WAV files are auto-converted):
  drumtool.js send kick.wav:0 snare.wav:1
  
  # Auto-increment slots (starts from 0):
  drumtool.js send kick.wav snare.wav hat.wav   # Slots 0,1,2 automatically
  
  # Transfer raw PCM data with a specific sample rate:
  drumtool.js send my_sample.pcm:10 22050 -v
  
  # Cannot mix formats - use either all file:slot OR all filenames
  drumtool.js version                           # Check firmware version
  drumtool.js format                            # Format filesystem
  drumtool.js reboot-bootloader                 # Enter bootloader mode`;

    console.log(usage);
  }
}

module.exports = {
  DrumtoolCli,
  DrumtoolCliError
};