#!/bin/sh
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" &> /dev/null && pwd)
pushd "$SCRIPT_DIR" > /dev/null
trap "popd > /dev/null" EXIT
if cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_VERBOSE_LOGGING=ON && cmake --build build --parallel 16; then
  # Find the generated .uf2 file (the name is now versioned)
  UF2_FILE=$(find build -name "*.uf2" -print -quit)

  if [ -f "$UF2_FILE" ]; then
    picotool load -f "$UF2_FILE"
  else
    echo "Error: .uf2 file not found in build directory."
    exit 1
  fi
else
  echo "Build failed. Not uploading."
  exit 1
fi