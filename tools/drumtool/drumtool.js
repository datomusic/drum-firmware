#!/usr/bin/env node
/**
 * Drumtool - DRUM Device Management Tool
 * 
 * Comprehensive tool for managing DATO DRUM devices via MIDI Sample Dump Standard (SDS).
 * This is the refactored version using modular architecture.
 * 
 * Features:
 * - Platform-agnostic core logic
 * - Modular architecture for easy testing and reusability  
 * - Clean separation of concerns
 * - Comprehensive error handling
 * - Progress monitoring and user feedback
 */

const { DrumtoolCli } = require('./src/cli/drumtool_cli');
const { NodeMidiPort } = require('./src/nodejs/node_midi_port');
const { NodeFileSystem } = require('./src/nodejs/node_file_system');
const { NodeUserInterface } = require('./src/nodejs/node_user_interface');

/**
 * Main entry point
 */
async function main() {
  // Create platform-specific implementations
  const midiPort = new NodeMidiPort();
  const fileSystem = new NodeFileSystem();
  const userInterface = new NodeUserInterface();
  
  // Create and run CLI
  const cli = new DrumtoolCli(midiPort, fileSystem, userInterface);
  const exitCode = await cli.run(process.argv);
  
  process.exit(exitCode);
}

// Handle unhandled promise rejections
process.on('unhandledRejection', (reason, promise) => {
  console.error('Unhandled Rejection at:', promise, 'reason:', reason);
  process.exit(1);
});

// Handle uncaught exceptions  
process.on('uncaughtException', (error) => {
  console.error('Uncaught Exception:', error);
  process.exit(1);
});

// Run the application
if (require.main === module) {
  main().catch((error) => {
    console.error('Fatal error:', error.message);
    process.exit(1);
  });
}

module.exports = {
  DrumtoolCli,
  NodeMidiPort,
  NodeFileSystem,  
  NodeUserInterface
};