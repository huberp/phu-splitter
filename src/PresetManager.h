#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>
#include <functional>
#include <optional>

class PhuSplitterAudioProcessor;

/**
 * Represents a single preset containing a name and crossover frequencies.
 */
struct Preset
{
    juce::String name;
    std::array<float, 6> frequencies;
};

/**
 * PresetManager: Handles loading, saving, deleting, and enumerating user presets.
 *
 * Presets are stored as individual XML files in a dedicated folder under the user's
 * application data directory. A built-in "Init" preset always exists and cannot be
 * deleted or overwritten.
 *
 * Thread safety: All methods must be called from the message thread only.
 */
class PresetManager
{
public:
    /** Construct a PresetManager linked to the given processor. */
    explicit PresetManager(PhuSplitterAudioProcessor& processor);

    // -- Queries --

    /** Get the list of all available preset names (Init is always first). */
    juce::StringArray getPresetNames() const;

    /** Get the currently active preset name. Empty string if none selected. */
    juce::String getCurrentPresetName() const { return currentPresetName; }

    /** Get the index of the current preset in the names list, or -1. */
    int getCurrentPresetIndex() const;

    /** Get the total number of presets. */
    int getNumPresets() const;

    // -- Actions --

    /** Load a preset by name. Applies its frequencies to the processor. */
    void loadPreset(const juce::String& name);

    /** Load a preset by index (0-based, where 0 = Init). */
    void loadPresetByIndex(int index);

    /** Save current processor frequencies as a new or overwritten preset. */
    void savePreset(const juce::String& name);

    /** Delete a user preset by name. Cannot delete "Init". */
    void deletePreset(const juce::String& name);

    /** Rename a user preset. Cannot rename "Init". */
    void renamePreset(const juce::String& oldName, const juce::String& newName);

    /** Reset processor frequencies to Init defaults. */
    void loadInit();

    /** Navigate to next preset; wraps around. */
    void loadNextPreset();

    /** Navigate to previous preset; wraps around. */
    void loadPreviousPreset();

    /** Callback for external listeners when preset list or selection changes. */
    std::function<void()> onPresetsChanged;

private:
    /** Get the directory where user presets are stored. Creates it if needed. */
    juce::File getPresetsDirectory() const;

    /** Scan the presets directory and rebuild the internal list. */
    void scanPresets();

    /** Write a Preset struct to an XML file. */
    void writePresetFile(const Preset& preset) const;

    /** Read a Preset from an XML file. Returns nullopt on failure. */
    std::optional<Preset> readPresetFile(const juce::File& file) const;

    /** Get the file path for a named preset. */
    juce::File getPresetFile(const juce::String& name) const;

    /** Apply frequencies from a Preset to the processor parameters. */
    void applyPreset(const Preset& preset);

    /** Read current processor frequencies into an array. */
    std::array<float, 6> readProcessorFrequencies() const;

    PhuSplitterAudioProcessor& processorRef;
    juce::String currentPresetName;
    juce::StringArray presetNames; // Always starts with "Init"

    static constexpr const char* INIT_PRESET_NAME = "Init";
    static constexpr const char* PRESET_FILE_EXTENSION = ".xml";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
