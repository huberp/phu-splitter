cmake_minimum_required(VERSION 3.15)
project(phu-splitter VERSION 0.5.0 LANGUAGES C CXX)

# Full version string with pre-release identifier for alpha/beta releases
set(PHU_SPLITTER_VERSION_SUFFIX "-alpha")
set(PHU_SPLITTER_FULL_VERSION "${PROJECT_VERSION}${PHU_SPLITTER_VERSION_SUFFIX}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE
# Build JUCE extras/examples OFF for faster builds
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Build JUCE Extras")
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Build JUCE Examples")

set(JUCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE root")
add_subdirectory("${JUCE_ROOT}")

add_subdirectory(phu-audio-lib)

# PhuNetworkLib and PhuAudioLib use JUCE headers but are not JUCE targets.
# Provide include directories and essential definitions so the headers compile
# without triggering a second JUCE module build.
target_include_directories(PhuNetworkLib SYSTEM PUBLIC ${JUCE_ROOT}/modules)
target_compile_definitions(PhuNetworkLib PUBLIC
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_MODULE_AVAILABLE_juce_gui_basics=1
    JUCE_MODULE_AVAILABLE_juce_graphics=1
    JUCE_MODULE_AVAILABLE_juce_events=1
    JUCE_MODULE_AVAILABLE_juce_data_structures=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

# PhuNetworkLib must be position-independent since phu-splitter builds as a shared library (VST3)
set_target_properties(PhuNetworkLib PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(src)