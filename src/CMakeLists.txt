juce_add_plugin(phu-arp
    PLUGIN_MANUFACTURER_CODE Phub
    PLUGIN_CODE Parp
    FORMATS VST3
    PRODUCT_NAME "PHU ARP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(phu-arp PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    EditorLogger.cpp
    EditorLogger.h
)

target_compile_definitions(phu-arp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CAMERA=0
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_USE_OGGVORBIS=0
    JUCE_USE_FLAC=0
)

target_link_libraries(phu-arp
    PRIVATE
        EventSystem
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)