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
                                                                                                                                                                                       
# Find all .wav files and process them                                                                                                                                                 
find "$SAMPLE_DIR" -maxdepth 1 -name "*.wav" -print0 | while IFS= read -r -d $'\0' wav_file; do                                                                                        
  # Get the filename without extension (e.g., AudioSampleKickc78_16bit_44kw)                                                                                                           
  base_name=$(basename "$wav_file" .wav)                                                                                                                                               
                                                                                                                                                                                       
  # Define output filenames                                                                                                                                                            
  pcm_file="$SAMPLE_DIR/${base_name}.pcm"                                                                                                                                              
  c_file="$SAMPLE_DIR/${base_name}_data.c" # Match CMakeLists.txt pattern                                                                                                              
                                                                                                                                                                                       
  # Define the C symbol base name (used by xxd)                                                                                                                                        
  # xxd replaces non-alphanumeric characters with underscores                                                                                                                          
  c_symbol_base=$(echo "$base_name" | tr -c '[:alnum:]_' '_')                                                                                                                          
                                                                                                                                                                                       
  echo "Processing: $base_name"                                                                                                                                                        
                                                                                                                                                                                       
  # 1. Convert WAV to raw PCM using sox                                                                                                                                                
  echo "  Converting to PCM: $pcm_file"                                                                                                                                                
  sox "$wav_file" -L -b 16 -c 1 -r 44100 -e signed-integer -t raw "$pcm_file"                                                                                                          
                                                                                                                                                                                       
  # 2. Convert PCM to C array using xxd                                                                                                                                                
  echo "  Generating C array: $c_file"                                                                                                                                                 
  # Use the C symbol base name for the array variable in the C file                                                                                                                    
  xxd -i "$pcm_file" | sed "s/unsigned char.*\[\]/unsigned char samples_${c_symbol_base}_pcm[]/" | sed "s/unsigned int.*len/unsigned int samples_${c_symbol_base}_pcm_len/" > "$c_file"
                                                                                                                                                                                       
  # 3. Print the base name for C++ usage                                                                                                                                               
  echo "$c_symbol_base" # Print the name like AudioSampleKickc78_16bit_44kw                                                                                                            
                                                                                                                                                                                       
  # Optional: Clean up the intermediate PCM file                                                                                                                                       
  # rm "$pcm_file"                                                                                                                                                                     
  # echo "  Removed intermediate PCM file."                                                                                                                                            
                                                                                                                                                                                       
done                                                                                                                                                                                   
                                                                                                                                                                                       
echo "----------------------------------------"                                                                                                                                        
echo "Script finished."                                                                                                                                                                
echo "Remember to update experiments/midi_sample_player/samples.h and experiments/midi_sample_player/CMakeLists.txt with the names listed above."                                      
                                                                                                                                                                                       
exit 0                                                                                                                                                                                 