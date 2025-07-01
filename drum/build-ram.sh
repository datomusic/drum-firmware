#!/bin/sh
if cmake -B build -DPICO_COPY_TO_RAM=ON -DENABLE_VERBOSE_LOGGING=OFF -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel 16; then
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
