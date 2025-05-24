#!/bin/bash

# Script to convert WAV samples to raw PCM, then to C arrays using xxd.
# Provides options to control output (PCM, C headers, or both).

# Best practices:
set -euo pipefail # Exit on error, undefined variable, or pipe failure

# --- Script Configuration ---
DEFAULT_OUTPUT_TYPE="c_header"
DEFAULT_OUTPUT_SUBDIR_NAME="generated_samples"

# --- Helper Functions ---
usage() {
  cat <<EOF
Usage: $(basename "$0") -s <sample_dir> [-o <output_dir>] [-t <type>] [-h]

Converts WAV samples to PCM and/or C header files.

Arguments:
  -s, --sample-dir DIR    Directory containing input .wav files. (Required)
  -o, --output-dir DIR    Directory to store generated files.
                          (Default: <sample_dir>/${DEFAULT_OUTPUT_SUBDIR_NAME} if -s is relative,
                           or ./<sample_dir_basename>/${DEFAULT_OUTPUT_SUBDIR_NAME} if -s is absolute)
  -t, --type TYPE         Output type:
                            pcm       - Generate only .pcm files.
                            c_header  - Generate only C header files (default).
                            both      - Generate both .pcm and C header files.
  -h, --help              Display this help message and exit.

Example:
  $(basename "$0") -s ./my_samples -t both
  $(basename "$0") -s ./audio_sources -o ./build/generated_headers
  $(basename "$0") -s /path/to/my_samples -t pcm
EOF
  exit 1
}

command_exists() {
  command -v "$1" >/dev/null 2>&1
}

# --- Argument Parsing ---
sample_dir_arg=""
output_dir_arg=""
output_type="$DEFAULT_OUTPUT_TYPE"

# Manual argument parsing loop
while [ "$#" -gt 0 ]; do
  case "$1" in
    -s|--sample-dir)
      if [ -n "$2" ] && [[ "$2" != -* ]]; then # Check if $2 exists and is not an option starting with -
        sample_dir_arg="$2"
        shift # Consume the argument value
      else
        echo "Error: Argument for $1 is missing or invalid." >&2
        usage >&2 # usage function already calls exit
      fi
      ;;
    -o|--output-dir)
      if [ -n "$2" ] && [[ "$2" != -* ]]; then
        output_dir_arg="$2"
        shift # Consume the argument value
      else
        echo "Error: Argument for $1 is missing or invalid." >&2
        usage >&2
      fi
      ;;
    -t|--type)
      if [ -n "$2" ] && [[ "$2" != -* ]]; then
        output_type="$2"
        shift # Consume the argument value
      else
        echo "Error: Argument for $1 is missing or invalid." >&2
        usage >&2
      fi
      ;;
    -h|--help)
      usage # usage function already calls exit
      ;;
    --) # End of options marker
      shift # Consume the --
      break # Stop processing options, remaining arguments are positional
      ;;
    -*) # Unknown option
      echo "Error: Unknown option: $1" >&2
      usage >&2
      ;;
    *)  # Positional argument or end of arguments
      # This script doesn't expect positional arguments here after all options are parsed.
      # If we encounter one, it's likely an error or options parsing is done.
      break
      ;;
  esac
  shift # Consume the current option/argument that was processed
done

# Validate required arguments
if [[ -z "$sample_dir_arg" ]]; then
  echo "Error: Sample directory (-s, --sample-dir) is required." >&2
  usage
fi

# Validate output type
if [[ "$output_type" != "pcm" && "$output_type" != "c_header" && "$output_type" != "both" ]]; then
  echo "Error: Invalid output type '$output_type'. Must be 'pcm', 'c_header', or 'both'." >&2
  usage
fi

# Determine sample directory (resolve to absolute path)
sample_dir=$(realpath "$sample_dir_arg")

# Determine output directory
if [[ -z "$output_dir_arg" ]]; then
  # Default output directory logic
  if [[ "$sample_dir_arg" == /* ]]; then # Absolute path
    output_dir="./$(basename "$sample_dir")/${DEFAULT_OUTPUT_SUBDIR_NAME}"
  else # Relative path
    output_dir="${sample_dir_arg%/}/${DEFAULT_OUTPUT_SUBDIR_NAME}"
  fi
else
  output_dir="${output_dir_arg%/}"
fi
output_dir=$(realpath -m "$output_dir") # Resolve and create if -m is supported, or use mkdir -p later

# --- Pre-flight Checks ---
if ! command_exists ffmpeg; then
  echo "Error: ffmpeg is not installed. Please install ffmpeg." >&2
  exit 1
fi

if ! command_exists xxd; then
  echo "Error: xxd is not installed. Please install xxd." >&2
  exit 1
fi

if [ ! -d "$sample_dir" ]; then
  echo "Error: Sample directory '$sample_dir' not found." >&2
  exit 1
fi

# Create output directory if it doesn't exist
mkdir -p "$output_dir"
echo "Source directory: '$sample_dir'"
echo "Output directory: '$output_dir'"
echo "Output type: '$output_type'"

# --- Initialization for Aggregate Headers (if generating C headers) ---
generate_c_headers=false
if [[ "$output_type" == "c_header" || "$output_type" == "both" ]]; then
  generate_c_headers=true
fi

sample_data_h_path="$output_dir/sample_data.h"
all_samples_h_path="$output_dir/all_samples.h"
sample_count=0
all_samples_entries_tmp="" # Will be created by mktemp if needed

if $generate_c_headers; then
  echo "Initializing C header generation..."
  cat > "$sample_data_h_path" <<EOF
// Generated by $(basename "$0")
#ifndef SAMPLE_DATA_H_GENERATED
#define SAMPLE_DATA_H_GENERATED

// This header assumes 'sample_conversion.h' is in the project's include path.
// It should provide a function like:
// template <unsigned int N>
// constexpr etl::array<int16_t, N / 2> int16FromBytes(const unsigned char (&bytes)[N]) { ... }
#include "sample_conversion.h"

EOF

  # Temporary file to store entries for all_samples.h
  all_samples_entries_tmp=$(mktemp)
  # Initial content for all_samples.h (preamble)
  cat > "$all_samples_h_path" <<EOF
// Generated by $(basename "$0")
#ifndef ALL_SAMPLES_H_GENERATED
#define ALL_SAMPLES_H_GENERATED

#include "sample_data.h" // Refers to the generated sample_data.h in the same directory
#include "etl/array.h"   // Assumed to be in project's include path

// Definition of the SampleData struct. Ensure this matches any existing project definitions.
struct SampleData {
    const int16_t* data;
    const uint32_t length; // Number of int16_t samples
};

// The array of all samples will be defined below, populated by the script.
EOF
fi

# --- Main Processing Loop ---
echo "Processing samples from '$sample_dir'..."
echo "----------------------------------------"

wav_files_list=()
while IFS= read -r -d $'\0'; do
    wav_files_list+=("$REPLY")
done < <(find "$sample_dir" -maxdepth 1 -name "*.wav" -print0)

if [ ${#wav_files_list[@]} -eq 0 ]; then
  echo "No .wav files found in '$sample_dir'."
else
  for wav_file_abs_path in "${wav_files_list[@]}"; do
    base_name=$(basename "$wav_file_abs_path" .wav)
    # xxd replaces non-alphanumeric characters with underscores.
    c_symbol_base=$(echo "$base_name" | tr -c '[:alnum:]_' '_')

    echo "Processing: $base_name (Symbol: samples_${c_symbol_base})"

    pcm_file_path="$output_dir/${base_name}.pcm"
    # Consistent naming for individual sample C header files
    c_header_file_path="$output_dir/samples_${c_symbol_base}_data.h"

    # 1. Convert WAV to raw PCM
    # This step is always needed if C headers are generated, or if PCM output is requested.
    needs_pcm_conversion=false
    if [[ "$output_type" == "pcm" || "$output_type" == "both" || "$generate_c_headers" == true ]]; then
      needs_pcm_conversion=true
    fi

    if $needs_pcm_conversion; then
      echo "  Converting to PCM: $pcm_file_path"
      ffmpeg -loglevel error -y -i "$wav_file_abs_path" -f s16le -acodec pcm_s16le "$pcm_file_path"
    fi

    # 2. Generate C header file (if requested)
    if $generate_c_headers; then
      echo "  Generating C array: $c_header_file_path"
      cat > "$c_header_file_path" <<EOF
// Generated for $base_name
#ifndef SAMPLES_${c_symbol_base}_DATA_H
#define SAMPLES_${c_symbol_base}_DATA_H

// For __in_flash() attribute. Assumed to be in project's include path.
#include "pico/platform/sections.h"

#ifdef __cplusplus
extern "C" {
#endif

EOF

      # Convert PCM to C array using xxd and modify declarations using sed
      xxd -i "$pcm_file_path" \
        | sed "s/unsigned char.*\[\]/const unsigned char __in_flash() samples_${c_symbol_base}_bytes[]/" \
        | sed "s/unsigned int.*len/const unsigned int samples_${c_symbol_base}_bytes_len/" >> "$c_header_file_path"

      cat >> "$c_header_file_path" <<EOF

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SAMPLES_${c_symbol_base}_DATA_H
EOF

      # Append include and constexpr definition to sample_data.h
      echo "#include \"samples_${c_symbol_base}_data.h\" // Individual sample data header" >> "$sample_data_h_path"
      echo "constexpr const auto samples_${c_symbol_base}_pcm = int16FromBytes<samples_${c_symbol_base}_bytes_len>(samples_${c_symbol_base}_bytes);" >> "$sample_data_h_path"
      echo "" >> "$sample_data_h_path" # Add a newline for readability

      # Append entry to the temporary file for all_samples.h
      echo "    SampleData{samples_${c_symbol_base}_pcm.begin(), samples_${c_symbol_base}_pcm.size()}," >> "$all_samples_entries_tmp"
      sample_count=$((sample_count + 1))
    fi

    # 3. Clean up intermediate PCM file if only C headers were requested and PCM was not explicitly asked for
    if [[ "$output_type" == "c_header" && -f "$pcm_file_path" ]]; then
      echo "  Cleaning up intermediate PCM: $pcm_file_path"
      rm "$pcm_file_path"
    elif [[ "$output_type" == "pcm" || "$output_type" == "both" ]]; then
      echo "  PCM file kept: $pcm_file_path"
    fi
    echo "  Done with $base_name."
  done
fi


# --- Finalize Aggregate Headers (if C headers were generated) ---
if $generate_c_headers; then
  echo "" >> "$sample_data_h_path" # Final newline for sample_data.h
  echo "#endif // SAMPLE_DATA_H_GENERATED" >> "$sample_data_h_path"

  # Append the all_samples array definition to all_samples.h
  echo "" >> "$all_samples_h_path" # Ensure newline before array definition
  echo "static constexpr const etl::array<SampleData, $sample_count> all_samples = {{" >> "$all_samples_h_path"
  if [ -s "$all_samples_entries_tmp" ]; then # Check if tmp file has content
    cat "$all_samples_entries_tmp" >> "$all_samples_h_path"
  fi
  echo "}};" >> "$all_samples_h_path"
  echo "" >> "$all_samples_h_path"
  echo "#endif // ALL_SAMPLES_H_GENERATED" >> "$all_samples_h_path"

  rm -f "$all_samples_entries_tmp" # Clean up temporary file

  echo ""
  echo "Generated C header files:"
  echo "  Individual sample data headers: $output_dir/samples_*_data.h"
  echo "  Aggregated sample data definitions: $sample_data_h_path"
  echo "  Central array of all samples: $all_samples_h_path"
  echo "Important: Ensure 'sample_conversion.h', 'etl/array.h', and Pico SDK headers (e.g., 'pico/platform/sections.h')"
  echo "are available in your C++ project's include paths."
fi

if [[ "$output_type" == "pcm" || "$output_type" == "both" ]]; then
  if [ ${#wav_files_list[@]} -gt 0 ]; then # Only print if files were processed
    echo ""
    echo "Generated PCM files in: $output_dir"
  fi
fi

echo "----------------------------------------"
echo "Script finished successfully."
exit 0
