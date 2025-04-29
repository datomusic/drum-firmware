#!/bin/zsh

# Script to convert WAV samples to raw PCM, then to C arrays using xxd,
# and list the base names for use in C++ code.

# Ensure sox and xxd are installed

set -e # Exit immediately if a command exits with a non-zero status.

# Define the directory containing the samples
SAMPLE_DIR="samples"

# Check if the sample directory exists
if [ ! -d "$SAMPLE_DIR" ]; then
  echo "Error: Sample directory '$SAMPLE_DIR' not found."
  exit 1
fi

echo "Processing samples in '$SAMPLE_DIR'..."
echo "----------------------------------------"
echo "Generated C Symbol Base Names:"
echo "(Use these to replace 'SampleNameX' in samples.h and CMakeLists.txt)"
echo "----------------------------------------"

sample_data_header="sample_data.h"
all_samples_header="all_samples.h"

echo "#include \"sample_conversion.h\"" > "$sample_data_header"
echo "#include \"sample_data.h\"" > "$all_samples_header"
echo "#include \"etl/array.h\"" >> "$all_samples_header"
echo "struct SampleData{const int16_t *data; const uint32_t length;};" >> "$all_samples_header"
echo "static constexpr const etl::array<SampleData, 51> all_samples = {" >> "$all_samples_header"

# Find all .wav files and process them
find "$SAMPLE_DIR" -maxdepth 1 -name "*.wav" -print0 | while IFS= read -r -d $'\0' wav_file; do
  # Get the filename without extension (e.g., AudioSampleKickc78_16bit_44kw)
  base_name=$(basename "$wav_file" .wav)

  # Define output filenames
  pcm_file="$SAMPLE_DIR/${base_name}.pcm"

  # Define the C symbol base name (used by xxd)
  # xxd replaces non-alphanumeric characters with underscores
  c_symbol_base=$(echo "$base_name" | tr -c '[:alnum:]_' '_')
  c_file="$SAMPLE_DIR/${c_symbol_base}data.h" # Match CMakeLists.txt pattern

  echo "Processing: $base_name"

  # 1. Convert WAV to raw PCM using sox
  echo "  Converting to PCM: $pcm_file"
  ffmpeg -i "$wav_file" -f s16le -acodec pcm_s16le "$pcm_file"

  # 2. Convert PCM to C array using xxd
  echo "  Generating C array: $c_file"
  # Use the C symbol base name for the array variable in the C file
  echo "#include \"pico/platform/sections.h"\" > "$c_file"
  xxd -i "$pcm_file" \
    | sed "s/unsigned char.*\[\]/constexpr const unsigned char __in_flash() samples_${c_symbol_base}_bytes[]/" \
    | sed "s/unsigned int.*len/constexpr const unsigned int samples_${c_symbol_base}_bytes_len/" >> "$c_file"

  echo "#include \"$SAMPLE_DIR/${c_symbol_base}data.h\"" >> "$sample_data_header"
  echo "constexpr const auto samples_${c_symbol_base}_pcm = int16FromBytes<samples_${c_symbol_base}_bytes_len>(samples_${c_symbol_base}_bytes);" >> "$sample_data_header"

  echo "SampleData{samples_${c_symbol_base}_pcm.begin(), samples_${c_symbol_base}_pcm.size()}," >> "$all_samples_header"

  # 3. Print the base name for C++ usage
  echo "$c_symbol_base" # Print the name like AudioSampleKickc78_16bit_44kw

  # Clean up the intermediate PCM file
  rm "$pcm_file"

done

echo "};" >> "$all_samples_header"

echo "----------------------------------------"
echo "Script finished."
echo "Remember to update experiments/midi_sample_player/samples.h and experiments/midi_sample_player/CMakeLists.txt with the names listed above."

exit 0
