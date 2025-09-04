#!/bin/bash

# Unified build script for drum firmware with A/B partition support
set -e

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" &> /dev/null && pwd)
pushd "$SCRIPT_DIR" > /dev/null
trap "popd > /dev/null" EXIT

# Function to check if device is connected and force BOOTSEL mode if needed
check_device_connected() {
  # Try to connect to device, force BOOTSEL mode if needed
  if ! picotool info >/dev/null 2>&1; then
    echo "No RP2350 device found in BOOTSEL mode, attempting to force BOOTSEL mode..."
        if ! picotool info -F >/dev/null 2>&1; then
      echo "Error: No RP2350 device found" >&2
      echo "Please connect the device and put it in BOOTSEL mode before uploading" >&2
      return 1
    fi
  fi
  return 0
}

# Default values
VERBOSE=false
COPY_TO_RAM=true
UPLOAD=true
PARTITION=""
HELP=false
CLEAN=false
WHITE_LABEL=false
SETUP_PARTITIONS=false

# Parse command line arguments
while getopts "vVrfp:nch-:" opt; do
  case $opt in
    v) VERBOSE=true ;;
    V) VERBOSE=true ;;
    r) COPY_TO_RAM=true ;;
    f) COPY_TO_RAM=false ;;  # flash build
    p) PARTITION="$OPTARG" ;;
    n) UPLOAD=false ;;       # no upload
    c) CLEAN=true ;;         # clean build
    h) HELP=true ;;
    -)
      case "$OPTARG" in
        verbose) VERBOSE=true ;;
        ram) COPY_TO_RAM=true ;;
        flash) COPY_TO_RAM=false ;;
        partition=*) PARTITION="${OPTARG#*=}" ;;
        no-upload) UPLOAD=false ;;
        clean) CLEAN=true ;;
        white-label) WHITE_LABEL=true ;;
        setup-partitions) SETUP_PARTITIONS=true ;;
        help) HELP=true ;;
        *) echo "Unknown option --$OPTARG" >&2; exit 1 ;;
      esac ;;
    \?) echo "Invalid option: -$OPTARG" >&2; exit 1 ;;
    :) echo "Option -$OPTARG requires an argument." >&2; exit 1 ;;
  esac
done

if [ "$HELP" = true ]; then
  cat << EOF
Usage: $0 [OPTIONS]

Build and optionally upload drum firmware with A/B partition support.

OPTIONS:
  -v, --verbose        Enable verbose logging
  -r, --ram            Copy to RAM (default)
  -f, --flash          Build for flash (no RAM copy)
  -p N, --partition=N  Upload to specific partition (0=A, 1=B)
  -n, --no-upload      Build only, don't upload
  -c, --clean          Remove build directory before building
  --white-label        Program OTP white-label data from drum/white-label.json
  --setup-partitions   Create and flash partition table from drum/partition_table.json
  -h, --help           Show this help

EXAMPLES:
  $0                          # Default: RAM build, auto-upload
  $0 --verbose --partition=1  # Verbose, upload to Firmware B
  $0 --flash --no-upload      # Flash build, no upload
  $0 -f -p 0 -v              # Flash build, upload to Firmware A, verbose
  $0 --clean --verbose        # Clean build with verbose logging

PARTITION INFO:
  Partition 0: Firmware A
  Partition 1: Firmware B  
  Partition 2: Data (filesystem)
  
  Without -p flag, uploads to currently inactive partition.

EOF
  exit 0
fi

# Validate partition argument
if [ -n "$PARTITION" ]; then
  if ! [[ "$PARTITION" =~ ^[0-1]$ ]]; then
    echo "Error: Partition must be 0 (Firmware A) or 1 (Firmware B)" >&2
    exit 1
  fi
fi

# Handle partition setup
if [ "$SETUP_PARTITIONS" = true ]; then
  PARTITION_JSON="$SCRIPT_DIR/partition_table.json"
  if [ ! -f "$PARTITION_JSON" ]; then
    echo "Error: partition_table.json not found at $PARTITION_JSON" >&2
    exit 1
  fi
  
  # Create build directory if it doesn't exist
  mkdir -p "$SCRIPT_DIR/build"
  
  PARTITION_UF2="$SCRIPT_DIR/build/partition_table.uf2"
  
  echo "Creating partition table from $PARTITION_JSON..."
  if ! picotool partition create "$PARTITION_JSON" "$PARTITION_UF2"; then
    echo "Error: Partition table creation failed" >&2
    exit 1
  fi
  
  echo "Flashing partition table..."
  if ! picotool load -f "$PARTITION_UF2"; then
    echo "Error: Partition table flash failed" >&2
    exit 1
  fi
  
  echo "Rebooting device to apply partition table..."
  if ! picotool reboot -f -u; then
    echo "Warning: Reboot command failed. Please reboot the device manually." >&2
  fi
  
  echo "Partition setup complete!"
  exit 0
fi

# Handle white-label programming
if [ "$WHITE_LABEL" = true ]; then
  WHITE_LABEL_JSON="$SCRIPT_DIR/white-label.json"
  if [ ! -f "$WHITE_LABEL_JSON" ]; then
    echo "Error: white-label.json not found at $WHITE_LABEL_JSON" >&2
    exit 1
  fi
  echo "Programming white-label OTP from $WHITE_LABEL_JSON..."
  if ! picotool otp white-label -s 0x400 "$WHITE_LABEL_JSON" -f; then
    echo "Error: White-label programming failed" >&2
    exit 1
  fi
  echo "White-label programming complete!"
  exit 0
fi

# Clean build directory if requested
if [ "$CLEAN" = true ]; then
  echo "Cleaning build directory..."
  rm -rf build
fi

# Set build configuration
BUILD_TYPE="Release"
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if [ "$COPY_TO_RAM" = true ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DPICO_COPY_TO_RAM=ON"
  echo "Building for RAM execution..."
else
  echo "Building for Flash execution..."
fi

if [ "$VERBOSE" = true ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DENABLE_VERBOSE_LOGGING=ON"
  echo "Verbose logging enabled"
else
  CMAKE_ARGS="$CMAKE_ARGS -DENABLE_VERBOSE_LOGGING=OFF"
fi

# Build
echo "Configuring build: cmake -B build $CMAKE_ARGS"
if ! cmake -B build $CMAKE_ARGS; then
  echo "Error: CMake configuration failed" >&2
  exit 1
fi

echo "Building with 16 parallel jobs..."
if ! cmake --build build --parallel 16; then
  echo "Error: Build failed" >&2
  exit 1
fi

# Find the generated UF2 file
UF2_FILE=$(find build -name "*.uf2" -print -quit)
if [ ! -f "$UF2_FILE" ]; then
  echo "Error: .uf2 file not found in build directory" >&2
  exit 1
fi

echo "Build successful: $UF2_FILE"

# Upload if requested
if [ "$UPLOAD" = false ]; then
  echo "Build complete. Skipping upload (--no-upload specified)."
  exit 0
fi

# Prepare picotool arguments
PICOTOOL_ARGS="-f" # Always force BOOTSEL mode

# Add partition if specified
if [ -n "$PARTITION" ]; then
  PICOTOOL_ARGS="$PICOTOOL_ARGS -p $PARTITION"
  echo "Uploading to partition $PARTITION..."
else
  echo "Uploading to default partition..."
fi

# For RAM builds, we typically want to execute immediately
if [ "$COPY_TO_RAM" = true ]; then
  PICOTOOL_ARGS="$PICOTOOL_ARGS -x"
fi

# Always verify partitions exist before flashing
echo "Verifying device partitions..."

# Check if device is connected first
if ! check_device_connected; then
  exit 1
fi



# Check if device has partitions at all
if ! picotool partition info -f 2>/dev/null | grep -q "^[[:space:]]*[0-9]("; then
  echo "Error: No partitions found on the connected device" >&2
  echo "This firmware requires a partitioned device. Please create partitions before flashing." >&2
  exit 1
fi

# If specific partition specified, verify it exists
if [ -n "$PARTITION" ]; then
  if ! picotool partition info -f 2>/dev/null | grep -q "^[[:space:]]*$PARTITION("; then
    echo "Error: Partition $PARTITION does not exist on the connected device" >&2
    echo "Available partitions:" >&2
    picotool partition info -f 2>/dev/null | grep "^[[:space:]]*[0-9](" >&2
    exit 1
  fi
  echo "Partition $PARTITION verified successfully"
else
  echo "No partition specified, will upload to default partition"
  echo "Available partitions:" >&2
  picotool partition info -f 2>/dev/null | grep "^[[:space:]]*[0-9](" >&2
fi

# Upload
echo "Uploading: picotool load \"$UF2_FILE\"$PICOTOOL_ARGS"
if ! picotool load "$UF2_FILE" $PICOTOOL_ARGS; then
  echo "Error: Upload failed" >&2
  exit 1
fi

echo "Upload complete!"
