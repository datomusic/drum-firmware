const midi = require('midi');
const fs = require('fs');

const output = new midi.Output();
const count = output.getPortCount();
console.log("Outputs: " + count);
const name = output.getPortName(1);
console.log("Name: " + name);
output.openPort(1);


function send_drum_message(tag, body) {
  output.sendMessage([0xF0].concat(
    [0, 0x7D, 0x65], // Manufacturer ID
    [0, 0, tag],
    body,
    [0xF7]));
}

function begin_file_transfer(file_name) {
  console.log("begin_file_transfer");
  send_drum_message(0x10, [0, 0, 0]); // TODO: Actually send file_name
}

function end_file_transfer() {
  console.log("end_file_transfer");
  send_drum_message(0x12, []);
}

function pack3_16(value) {
  return [
      (value >> 14) & 0x7F,
      (value >> 7) & 0x7F,
      value & 0x7F
  ];
}

const sleepMs = (milliseconds) => new Promise(resolve => setTimeout(resolve, milliseconds));

async function send_file_content(data) {
  console.log("send_file_content");
  const ChunkSize = 50;
  console.log("File data length: ", data.length);

  var bytes = [];

  var chunk_counter = 0;
  var i =0;
  for (i=0;i<data.length;i+=2) {
    // Pack two bytes into a 16bit value, and then into 3 sysex bytes.
    const lower = data[i]
    const upper = data[i+1]
    bytes = bytes.concat(pack3_16((upper << 8) + lower));


    // Stay below the max SysEx message length for DRUM, leaving space for header.
    if (bytes.length >= 100) {
      console.log("Sending chunk "+chunk_counter);
      chunk_counter++;
      send_drum_message(0x11, bytes)
      bytes = [];

      // Don't overload buffers of the DRUM
      await sleepMs(30)
    }

  }

  if (i != data.length) {
    console.warn("Warning: Skipped last data byte");
  }

  await sleepMs(100)
}


begin_file_transfer("test_file");
const data = fs.readFileSync('../../experiments/support/samples/002.pcm');

send_file_content(data).then( () => {
  end_file_transfer();
  output.closePort();
})

