#!/bin/bash

# Unified build script for drum firmware with A/B partition support
set -e

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" &> /dev/null && pwd)
pushd "$SCRIPT_DIR" > /dev/null
trap "popd > /dev/null" EXIT

# Default values
VERBOSE=false
COPY_TO_RAM=true
UPLOAD=true
PARTITION=""
FORCE_BOOTSEL=false
HELP=false
CLEAN=false

# Parse command line arguments
while getopts "vVrfp:nxch-:" opt; do
  case $opt in
    v) VERBOSE=true ;;
    V) VERBOSE=true ;;
    r) COPY_TO_RAM=true ;;
    f) COPY_TO_RAM=false ;;  # flash build
    p) PARTITION="$OPTARG" ;;
    n) UPLOAD=false ;;       # no upload
    x) FORCE_BOOTSEL=true ;; # force bootsel
    c) CLEAN=true ;;         # clean build
    h) HELP=true ;;
    -)
      case "$OPTARG" in
        verbose) VERBOSE=true ;;
        ram) COPY_TO_RAM=true ;;
        flash) COPY_TO_RAM=false ;;
        partition=*) PARTITION="${OPTARG#*=}" ;;
        no-upload) UPLOAD=false ;;
        force-bootsel) FORCE_BOOTSEL=true ;;
        clean) CLEAN=true ;;
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
  -x, --force-bootsel  Force device into BOOTSEL mode before upload
  -c, --clean          Remove build directory before building
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
PICOTOOL_ARGS=""

if [ "$FORCE_BOOTSEL" = true ]; then
  PICOTOOL_ARGS="$PICOTOOL_ARGS -f"
fi

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

# Verify partition exists if specified
if [ -n "$PARTITION" ]; then
  echo "Verifying partition $PARTITION exists..."
  if ! picotool info -a 2>/dev/null | grep -q "partition $PARTITION:"; then
    echo "Warning: Partition $PARTITION may not exist. Proceeding anyway..."
  fi
fi

# Upload
echo "Uploading: picotool load \"$UF2_FILE\"$PICOTOOL_ARGS"
if ! picotool load "$UF2_FILE" $PICOTOOL_ARGS; then
  echo "Error: Upload failed" >&2
  exit 1
fi

echo "Upload complete!"