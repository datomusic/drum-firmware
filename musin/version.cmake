# version.cmake - Generates version information from git
cmake_minimum_required(VERSION 3.18)

# Function to extract version information from git
function(configure_version_from_git)
  # Find Git executable
  find_package(Git REQUIRED)
  
  # Set default values in case git commands fail
  set(VERSION_MAJOR 0)
  set(VERSION_MINOR 0)
  set(VERSION_PATCH 0)
  set(VERSION_COMMITS 0)
  set(VERSION_SHA "unknown")
  set(VERSION_IS_RELEASE FALSE)
  
  # Get the latest tag
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  
  if(GIT_TAG)
    # Remove 'v' prefix if present
    string(REGEX REPLACE "^v" "" VERSION_TAG ${GIT_TAG})
    
    # Extract semver components
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH ${VERSION_TAG})
    
    if(VERSION_MATCH)
      set(VERSION_MAJOR ${CMAKE_MATCH_1})
      set(VERSION_MINOR ${CMAKE_MATCH_2})
      set(VERSION_PATCH ${CMAKE_MATCH_3})
    endif()
    
    # Count commits since the latest tag
    execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-list ${GIT_TAG}..HEAD --count
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE VERSION_COMMITS
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    
    # Get current commit hash
    execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE VERSION_SHA
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    
    # Check if we're exactly on a tag (release build)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} describe --exact-match --tags
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE IS_EXACT_TAG
      ERROR_VARIABLE IS_EXACT_TAG_ERROR
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_STRIP_TRAILING_WHITESPACE
    )
    
    if(IS_EXACT_TAG AND NOT IS_EXACT_TAG_ERROR)
      set(VERSION_IS_RELEASE TRUE)
    endif()
  endif()
  
  # Construct version strings
  set(VERSION_BASE "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
  
  if(VERSION_IS_RELEASE)
    set(VERSION_STRING "${VERSION_BASE}")
    set(VERSION_FILENAME "${VERSION_BASE}")
  else()
    set(VERSION_STRING "${VERSION_BASE}-dev.${VERSION_COMMITS}+${VERSION_SHA}")
    set(VERSION_FILENAME "${VERSION_BASE}-dev.${VERSION_COMMITS}-${VERSION_SHA}")
  endif()
  
  # Output version information to parent scope
  set(VERSION_MAJOR ${VERSION_MAJOR} PARENT_SCOPE)
  set(VERSION_MINOR ${VERSION_MINOR} PARENT_SCOPE)
  set(VERSION_PATCH ${VERSION_PATCH} PARENT_SCOPE)
  set(VERSION_COMMITS ${VERSION_COMMITS} PARENT_SCOPE)
  set(VERSION_SHA ${VERSION_SHA} PARENT_SCOPE)
  set(VERSION_STRING ${VERSION_STRING} PARENT_SCOPE)
  set(VERSION_FILENAME ${VERSION_FILENAME} PARENT_SCOPE)
  set(VERSION_BASE ${VERSION_BASE} PARENT_SCOPE)
  set(VERSION_IS_RELEASE ${VERSION_IS_RELEASE} PARENT_SCOPE)
  
  # Create packed version (Major: 4 bits, Minor: 6 bits, Patch: 4 bits)
  set(_MAJOR ${VERSION_MAJOR})
  set(_MINOR ${VERSION_MINOR})
  set(_PATCH ${VERSION_PATCH})

  # Clamp values and issue warnings if necessary
  if(_MAJOR GREATER 15)
    message(WARNING "Major version (${_MAJOR}) exceeds limit (15). Clamping to 15.")
    set(_MAJOR 15)
  endif()
  if(_MINOR GREATER 63)
    message(WARNING "Minor version (${_MINOR}) exceeds limit (63). Clamping to 63.")
    set(_MINOR 63)
  endif()
  if(_PATCH GREATER 15) # Patch uses 4 bits (0-15)
    message(WARNING "Patch version (${_PATCH}) exceeds limit (15). Clamping to 15.")
    set(_PATCH 15)
  endif()

  # Calculate the packed value using bitwise operations
  # (Major in top 4 bits) << 12 | (Minor in next 6 bits) << 6 | (Patch in bottom 4 bits)
  math(EXPR _CALCULATED_PACKED_VERSION "(${_MAJOR} << 12) | (${_MINOR} << 6) | ${_PATCH}")

  # Check if calculation resulted in an empty string (should not happen with clamping and default values)
  if("${_CALCULATED_PACKED_VERSION}" STREQUAL "")
    set(VERSION_BCD_VALUE "0") # Default to 0 if something unexpected happened
    message(WARNING "Could not compute packed version (MAJOR='${_MAJOR}', MINOR='${_MINOR}', PATCH='${_PATCH}'). Using default value 0 (0x0000).")
  else()
    set(VERSION_BCD_VALUE ${_CALCULATED_PACKED_VERSION})
  endif()

  set(VERSION_BCD ${VERSION_BCD_VALUE} PARENT_SCOPE)

  # Format the decimal packed value as a 4-digit hex string for display
  string(HEX ${VERSION_BCD_VALUE} VERSION_PACKED_HEX_STRING)

  # Display version info during cmake configuration
  message(STATUS "Firmware Version: ${VERSION_STRING}")
  # Display the intended 4-digit hex representation of the packed version
  message(STATUS "Firmware Packed Version (for USB bcdDevice): 0x${VERSION_PACKED_HEX_STRING}")
endfunction()

# Configure version header file
function(generate_version_header HEADER_FILE)
  # Version variables should already be set by calling configure_version_from_git()
  # in the main CMakeLists.txt before calling this function.
  
  # Ensure destination directory exists
  get_filename_component(_hdr_dir ${HEADER_FILE} DIRECTORY)
  file(MAKE_DIRECTORY ${_hdr_dir})

  # Compose header content
  set(_version_header_content "#ifndef FIRMWARE_VERSION_H\n")
  string(APPEND _version_header_content "#define FIRMWARE_VERSION_H\n\n")
  string(APPEND _version_header_content "#define FIRMWARE_VERSION \"${VERSION_STRING}\"\n")
  string(APPEND _version_header_content "#define FIRMWARE_VERSION_BASE \"${VERSION_BASE}\"\n")
  string(APPEND _version_header_content "#define FIRMWARE_MAJOR ${VERSION_MAJOR}\n")
  string(APPEND _version_header_content "#define FIRMWARE_MINOR ${VERSION_MINOR}\n")
  string(APPEND _version_header_content "#define FIRMWARE_PATCH ${VERSION_PATCH}\n")
  string(APPEND _version_header_content "#define FIRMWARE_COMMITS ${VERSION_COMMITS}\n")
  string(APPEND _version_header_content "#define FIRMWARE_COMMIT \"${VERSION_SHA}\"\n")
  string(APPEND _version_header_content "#define FIRMWARE_IS_RELEASE ${VERSION_IS_RELEASE}\n")
  string(APPEND _version_header_content "\n#endif // FIRMWARE_VERSION_H\n")

  # Only write if content changed to avoid needless recompiles
  set(_needs_write TRUE)
  if(EXISTS ${HEADER_FILE})
    file(READ ${HEADER_FILE} _existing_content)
    if(_existing_content STREQUAL _version_header_content)
      set(_needs_write FALSE)
    endif()
  endif()

  if(_needs_write)
    file(WRITE ${HEADER_FILE} "${_version_header_content}")
  endif()
endfunction()
