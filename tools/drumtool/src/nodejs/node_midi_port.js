/**
 * Node.js MIDI Port Implementation
 * 
 * Implementation of MIDI port interfaces using the 'midi' npm package.
 */

const midi = require('midi');
const { IMidiPort, IMidiInput, IMidiOutput } = require('../interfaces/midi_port');

class NodeMidiInput extends IMidiInput {
  constructor(midiInput) {
    super();
    this.midiInput = midiInput;
    this.messageHandler = null;
  }

  onMessage(handler) {
    this.messageHandler = handler;
    this.midiInput.on('message', handler);
  }

  removeMessageHandler() {
    if (this.messageHandler) {
      this.midiInput.removeListener('message', this.messageHandler);
      this.messageHandler = null;
    }
  }
}

class NodeMidiOutput extends IMidiOutput {
  constructor(midiOutput) {
    super();
    this.midiOutput = midiOutput;
  }

  async sendMessage(message) {
    // Convert to array if it's not already
    const messageArray = Array.isArray(message) ? message : Array.from(message);
    this.midiOutput.sendMessage(messageArray);
  }
}

class NodeMidiPort extends IMidiPort {
  constructor() {
    super();
    this.output = null;
    this.input = null;
    this.outputPort = null;
    this.inputPort = null;
    this.portName = null;
  }

  async connect(validPortNames) {
    const output = new midi.Output();
    const input = new midi.Input();
    const outPortCount = output.getPortCount();
    const inPortCount = input.getPortCount();

    let outPort = -1;
    let inPort = -1;
    let portName = "";

    // Find output port
    for (let i = 0; i < outPortCount; i++) {
      const currentPortName = output.getPortName(i);
      if (validPortNames.some(name => currentPortName.includes(name))) {
        outPort = i;
        portName = currentPortName;
        break;
      }
    }

    if (outPort === -1) {
      const availablePorts = [];
      for (let i = 0; i < outPortCount; i++) {
        availablePorts.push(`${i}: ${output.getPortName(i)}`);
      }
      
      const errorMsg = `No suitable MIDI output port found containing any of: ${validPortNames.join(", ")}.`;
      const fullError = availablePorts.length > 0 ? 
        `${errorMsg}\nAvailable MIDI output ports:\n${availablePorts.join('\n')}` :
        `${errorMsg}\nNo MIDI output ports available.`;
      
      throw new Error(fullError);
    }

    // Find matching input port
    for (let i = 0; i < inPortCount; i++) {
      if (input.getPortName(i).includes(portName)) {
        inPort = i;
        break;
      }
    }

    if (inPort === -1) {
      throw new Error(`Found MIDI output "${portName}", but no matching input port.`);
    }

    try {
      output.openPort(outPort);
      input.openPort(inPort);
      input.ignoreTypes(false, false, false);

      this.output = output;
      this.input = input;
      this.outputPort = outPort;
      this.inputPort = inPort;
      this.portName = portName;

      return {
        input: new NodeMidiInput(input),
        output: new NodeMidiOutput(output)
      };
    } catch (error) {
      throw new Error(`Error opening MIDI ports for ${portName}: ${error.message}`);
    }
  }

  async disconnect() {
    if (this.output && this.output.isPortOpen()) {
      this.output.closePort();
    }
    if (this.input && this.input.isPortOpen()) {
      this.input.closePort();
    }

    this.output = null;
    this.input = null;
    this.outputPort = null;
    this.inputPort = null;
    this.portName = null;
  }

  isConnected() {
    return this.output && this.output.isPortOpen() && 
           this.input && this.input.isPortOpen();
  }

  getPortName() {
    return this.portName;
  }
}

module.exports = {
  NodeMidiPort,
  NodeMidiInput,
  NodeMidiOutput
};