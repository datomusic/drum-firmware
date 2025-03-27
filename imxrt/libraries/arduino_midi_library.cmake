include_guard(GLOBAL)
message("arduino_midi_library component is included.")

set(MIDI_LIB_SRC ${CMAKE_CURRENT_LIST_DIR}/arduino_midi_library/src)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
${MIDI_LIB_SRC}/midi_Namespace.h
${MIDI_LIB_SRC}/midi_Defs.h
${MIDI_LIB_SRC}/midi_Message.h
${MIDI_LIB_SRC}/midi_Platform.h
${MIDI_LIB_SRC}/midi_Settings.h
${MIDI_LIB_SRC}/MIDI.cpp
${MIDI_LIB_SRC}/MIDI.hpp
${MIDI_LIB_SRC}/MIDI.h
${MIDI_LIB_SRC}/serialMIDI.h
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PRIVATE
    ${MIDI_LIB_SRC}/.
)

include(driver_common)
