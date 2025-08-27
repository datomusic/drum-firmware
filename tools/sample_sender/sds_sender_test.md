# SDS Sender Test Commands

## Test Commands and Expected Results

### 1. Help Output Test
```bash
node tools/sample_sender/sds_sender.js
```
**Expected:** Shows usage information and exits with code 1

### 2. Version Command Test
```bash
node tools/sample_sender/sds_sender.js version
```
**Expected:** 
- "Initializing MIDI connection..."
- "Opened MIDI ports: DRUM" 
- "Device firmware version: v0.7.0"
- Clean exit

### 3. Format Command Test
```bash
node tools/sample_sender/sds_sender.js format
```
**Expected:**
- "Initializing MIDI connection..."
- "Opened MIDI ports: DRUM"
- "Sending command to format filesystem..."
- "Successfully sent format command. The device will now re-initialize its filesystem."
- Clean exit

### 4. Invalid Command Tests
```bash
node tools/sample_sender/sds_sender.js invalid
node tools/sample_sender/sds_sender.js xyz
```
**Expected:** "Error: Unknown command 'invalid'. Use 'send', 'version', or 'format'." (fast exit, no MIDI init)

### 5. Send Command Validation Tests
```bash
# No arguments
node tools/sample_sender/sds_sender.js send
```
**Expected:** "Error: 'send' command requires at least one file:slot argument." (fast exit)

```bash
# Invalid format
node tools/sample_sender/sds_sender.js send invalidformat
```
**Expected:** "Error: Invalid format 'invalidformat'. Expected 'file:slot' format." (fast exit)

```bash
# Invalid slot number (too high)
node tools/sample_sender/sds_sender.js send test.wav:999
```
**Expected:** "Error: Invalid slot number '999'. Must be 0-127." (fast exit)

```bash
# Non-numeric slot
node tools/sample_sender/sds_sender.js send test.wav:abc
```
**Expected:** "Error: Invalid slot number 'abc'. Must be 0-127." (fast exit)

## Key Validation Points
- ✅ All argument validation happens **before** MIDI initialization
- ✅ Invalid commands/arguments exit immediately with helpful error messages
- ✅ Valid commands successfully initialize MIDI and communicate with device
- ✅ Version command returns actual firmware version from device
- ✅ Format command successfully sends format command to device