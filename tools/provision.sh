#!/bin/bash
#
# Production Provisioning Script for the Dato DRUM
#
# This script automates the full provisioning process for a blank RP2350.
# It is designed to be run from the project root directory.
#
# WARNING: This script is destructive and will erase the connected device.
#
# Assumes:
# - A pre-built firmware UF2 file exists.
# - The script is run on macOS (for device detection).
# - `picotool` is in the system PATH.
# - `node` is in the system PATH.

set -e # Exit immediately if a command exits with a non-zero status.

# --- Configuration ---
# Path to the firmware file to upload.
FIRMWARE_UF2="drum/build/drum.uf2" 
# Directory containing samples to upload. Assumes files are named 00_kick.wav, 01_snare.wav etc.
SAMPLES_DIR="support/samples/factory_kit"
# The name of the MIDI device to wait for.
MIDI_DEVICE_NAME="DRUM"
# The volume label of the white-labeled bootloader.
BOOTLOADER_VOLUME_NAME="DRUMBOOT"
# Timeout in seconds for device detection loops.
TIMEOUT_SECONDS=30

# --- Helper Functions ---

# Waits for the RP2350 to appear in BOOTSEL mode.
wait_for_bootsel() {
  echo "Waiting for device in BOOTSEL mode..."
  local elapsed=0
  while ! picotool info >/dev/null 2>&1; do
    if [ $elapsed -ge $TIMEOUT_SECONDS ]; then
      echo "Error: Timed out after ${TIMEOUT_SECONDS}s waiting for BOOTSEL mode." >&2
      return 1
    fi
    sleep 1
    elapsed=$((elapsed + 1))
    echo -n "."
  done
  echo "
Device found in BOOTSEL mode."
  return 0
}

# Waits for a USB Mass Storage volume to appear with a given name.
# This is specific to macOS.
wait_for_volume() {
  local volume_name="$1"
  echo "Waiting for volume '$volume_name' to mount..."
  local elapsed=0
  while [ ! -d "/Volumes/$volume_name" ]; do
    if [ $elapsed -ge $TIMEOUT_SECONDS ]; then
      echo "Error: Timed out after ${TIMEOUT_SECONDS}s waiting for volume." >&2
      return 1
    fi
    sleep 1
    elapsed=$((elapsed + 1))
    echo -n "."
  done
  echo "
Volume '$volume_name' found."
  return 0
}

# ---
# FUTURE IMPROVEMENT for drumtool.js:
# To make this script cross-platform and more robust, a 'wait-for-device'
# command could be added to drumtool.js. It would use the 'midi' library
# to check for the device and exit with 0 if found, 1 otherwise.
#
# The bash loop would then become:
# while ! node tools/drumtool/drumtool.js wait-for-device; do
#   sleep 1
# done
# ---
# Waits for a USB device to appear with a given name.
# This implementation is specific to macOS.
wait_for_usb_device() {
  local device_name="$1"
  echo "Waiting for USB device '$device_name' to appear..."
  local elapsed=0
  while ! system_profiler SPUSBDataType | grep -q "$device_name"; do
    if [ $elapsed -ge $TIMEOUT_SECONDS ]; then
      echo "Error: Timed out after ${TIMEOUT_SECONDS}s waiting for USB device." >&2
      return 1
    fi
    sleep 1
    elapsed=$((elapsed + 1))
    echo -n "."
  done
  echo "
USB device '$device_name' found."
  return 0
}


# --- Main Script ---

# Ensure we are in the project root
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" &> /dev/null && pwd)
cd "$SCRIPT_DIR/.."
echo "Running from project root: $(pwd)"

echo "--- Starting Dato DRUM Provisioning ---"

# 1. Check for device in bootloader mode
wait_for_bootsel
picotool info

# 2. White label the bootloader
echo "
--- Step 2: White-labeling bootloader ---"
# Note: This is a one-time, irreversible operation on the RP2350's OTP memory.
if ! picotool otp white-label -s 0x400 drum/white-label.json -f; then
    echo "Error: White-label programming failed. This may be because it has already been programmed." >&2
    # This might not be a fatal error in all cases, so we continue.
fi
echo "White-labeling complete."

# 3. Restart and verify white labeling
echo "
--- Step 3: Verifying white-label ---"
picotool reboot -u # Reboot into bootloader mode
wait_for_volume "$BOOTLOADER_VOLUME_NAME"
echo "Verified: Volume '$BOOTLOADER_VOLUME_NAME' is present."
sleep 2

# 4. Partition the device
echo "
--- Step 4: Partitioning device ---"
# The --setup-partitions command flashes the partition table and reboots.
./drum/build.sh --setup-partitions
echo "Partitioning complete. Device is rebooting."
sleep 2

# 5. Upload firmware
echo "
--- Step 5: Uploading firmware ---"
wait_for_bootsel # Wait for device to reappear in bootloader mode after partitioning
sleep 2
if [ ! -f "$FIRMWARE_UF2" ]; then
    echo "Error: Firmware file not found at $FIRMWARE_UF2" >&2
    echo "Please build the firmware first using ./drum/build.sh" >&2
    exit 1
fi
echo "Uploading $FIRMWARE_UF2..."
picotool load "$FIRMWARE_UF2" -f
picotool reboot -f
echo "Firmware upload complete. Device is rebooting into main application."

# 6. Format the filesystem
echo "
--- Step 6: Formatting filesystem ---"
wait_for_usb_device "$MIDI_DEVICE_NAME"
sleep 2
echo "Device is running. Sending format command..."
# The --no-input flag can be added to drumtool.js to skip the confirmation prompt
node tools/drumtool/drumtool.js format --no-input

# 7. Upload samples
echo "
--- Step 7: Uploading default samples ---"
if [ ! -d "$SAMPLES_DIR" ]; then
    echo "Warning: Samples directory not found at $SAMPLES_DIR. Skipping sample upload." >&2
else
    # This assumes drumtool.js can take multiple files and auto-assigns slots.
    # The shell expands the glob into a list of files.
    echo "Uploading samples from $SAMPLES_DIR..."
    node tools/drumtool/drumtool.js send ${SAMPLES_DIR}/*
    echo "Sample upload complete."
fi

# 8. Verify firmware and samples
echo "
--- Step 8: Verifying installation ---"
echo "Checking firmware version..."
node tools/drumtool/drumtool.js version

# ---
# FUTURE IMPROVEMENT for drumtool.js:
# To properly verify samples, a 'list-files' or 'verify-samples'
# command could be added to drumtool.js. It would query the device
# for a list of stored samples and their checksums.
#
# The bash script could then compare the returned list with the
# local files to confirm a successful upload.
# ---
echo "Verification step: Checking for firmware version is a basic check."
echo "A full sample verification would require extending drumtool.js."

echo "
--- Provisioning Complete! ---"
