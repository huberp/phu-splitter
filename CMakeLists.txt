cmake_minimum_required(VERSION 3.28)
project(phu-splitter VERSION 0.5.0 LANGUAGES C CXX)

# Full version string with pre-release identifier for alpha/beta releases
set(PHU_SPLITTER_VERSION_SUFFIX "-alpha")
set(PHU_SPLITTER_FULL_VERSION "${PROJECT_VERSION}${PHU_SPLITTER_VERSION_SUFFIX}")

# Global settings (before fetching, so they also apply to fetched dependencies)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE feature flags and build options; included before the fetch so that the
# options and compile definitions also reach JUCE and phu-audio-lib targets.
include(cmake/juce_config.cmake)

# Fetch dependencies (reusable helpers in cmake/PhuDependencies.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PhuDependencies)
phu_fetch_juce(TAG 8.0.15)
phu_fetch_audio_lib(TAG main)

# Warnings/sanitizers/coverage; included after the fetch so they only apply to this project
include(cmake/compiler_flags.cmake)

# PhuNetworkLib's .cpp files include juce_core headers; link the JUCE
# target that the parent project already provides.
target_link_libraries(PhuNetworkLib PUBLIC juce::juce_core)

add_subdirectory(src)