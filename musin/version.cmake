# version.cmake - Generates version information from git
cmake_minimum_required(VERSION 3.10)

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
  else()
    set(VERSION_STRING "${VERSION_BASE}-dev.${VERSION_COMMITS}+${VERSION_SHA}")
  endif()
  
  # Output version information to parent scope
  set(VERSION_MAJOR ${VERSION_MAJOR} PARENT_SCOPE)
  set(VERSION_MINOR ${VERSION_MINOR} PARENT_SCOPE)
  set(VERSION_PATCH ${VERSION_PATCH} PARENT_SCOPE)
  set(VERSION_COMMITS ${VERSION_COMMITS} PARENT_SCOPE)
  set(VERSION_SHA ${VERSION_SHA} PARENT_SCOPE)
  set(VERSION_STRING ${VERSION_STRING} PARENT_SCOPE)
  set(VERSION_BASE ${VERSION_BASE} PARENT_SCOPE)
  set(VERSION_IS_RELEASE ${VERSION_IS_RELEASE} PARENT_SCOPE)
  
  # Create BCD version (MM.mm -> 0xMMmm)
  # Ensure major and minor are treated as decimal numbers for the calculation
  math(EXPR VERSION_BCD "(${VERSION_MAJOR} * 256) + ${VERSION_MINOR}")
  set(VERSION_BCD ${VERSION_BCD} PARENT_SCOPE)

  # Display version info during cmake configuration
  message(STATUS "Firmware Version: ${VERSION_STRING}")
  message(STATUS "Firmware BCD Version: 0x${VERSION_BCD}")
endfunction()

# Configure version header file
function(generate_version_header HEADER_FILE)
  configure_version_from_git()
  
  # Create header file content
  file(WRITE ${HEADER_FILE} "#ifndef FIRMWARE_VERSION_H\n")
  file(APPEND ${HEADER_FILE} "#define FIRMWARE_VERSION_H\n\n")
  file(APPEND ${HEADER_FILE} "#define FIRMWARE_VERSION \"${VERSION_STRING}\"\n")
  file(APPEND ${HEADER_FILE} "#define FIRMWARE_VERSION_BASE \"${VERSION_BASE}\"\n")
  file(APPEND ${HEADER_FILE} "#define FIRMWARE_MAJOR ${VERSION_MAJOR}\n")
  file(APPEND ${HEADER_FILE} "#define FIRMWARE_MINOR ${VERSION_MINOR}\n")
  file(APPEND ${HEADER_FILE} "#define FIRMWARE_PATCH ${VERSION_PATCH}\n")
  file(APPEND ${HEADER_FILE} "#define FIRMWARE_COMMITS ${VERSION_COMMITS}\n")
  file(APPEND ${HEADER_FILE} "#define FIRMWARE_COMMIT \"${VERSION_SHA}\"\n")
  file(APPEND ${HEADER_FILE} "#define FIRMWARE_IS_RELEASE ${VERSION_IS_RELEASE}\n")
  file(APPEND ${HEADER_FILE} "\n#endif // FIRMWARE_VERSION_H\n")
endfunction()
