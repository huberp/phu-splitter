# Semicolon-separated JUCE formats; set by the "formats-*" presets in CMakePresets.json.
if(APPLE)
    set(_phu_default_formats "VST3;AU")
else()
    set(_phu_default_formats "VST3")
endif()
set(PHU_PLUGIN_FORMATS "${_phu_default_formats}" CACHE STRING "JUCE plugin formats to build (e.g. \"VST3;AU\")")

juce_add_plugin(phu-splitter
    PLUGIN_MANUFACTURER_CODE Phub
    PLUGIN_CODE Pspl
    FORMATS ${PHU_PLUGIN_FORMATS}
    PRODUCT_NAME "PHU SPLITTER"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(phu-splitter PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    BandControlStrip.cpp
    BandControlStrip.h
    BandWaveformDisplay.cpp
    BandWaveformDisplay.h
    CrossoverFrequencyBar.cpp
    CrossoverFrequencyBar.h
    PresetManager.cpp
    PresetManager.h
    PresetStrip.cpp
    PresetStrip.h
)

target_link_libraries(phu-splitter
    PRIVATE
        PhuAudioLib
        PhuNetworkLib
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Intel MKL support
if(USE_INTEL_MKL AND MKL_FOUND)
    target_compile_definitions(phu-splitter PRIVATE JUCE_DSP_USE_INTEL_MKL=1)
    target_link_libraries(phu-splitter PRIVATE PHU_MKL PUBLIC juce::juce_recommended_lto_flags)
    message(STATUS "Intel MKL FFT support enabled for phu-splitter")
endif()