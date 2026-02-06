#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetManager.h"

class PhuSplitterAudioProcessor;

/**
 * PresetStrip: Inline preset navigation / management bar (Design 3).
 *
 * Layout:  [<] [ Preset Name ▼ ] [>]   [Save] [+New] [Init]
 *
 * - [<] / [>] arrows step through presets sequentially (wrapping).
 * - Clicking the preset name opens a dropdown with all presets plus
 *   "Delete" and "Rename" actions for the current preset.
 * - [Save]  overwrites the current preset with current crossover values.
 * - [+New]  opens a dialog to name and save a new preset.
 * - [Init]  resets all crossover frequencies to factory defaults.
 */
class PresetStrip : public juce::Component
{
public:
    explicit PresetStrip(PhuSplitterAudioProcessor& processor);
    ~PresetStrip() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    /** Rebuild the preset name label from PresetManager state. */
    void updatePresetNameDisplay();

    /** Show the dropdown with preset list + management actions. */
    void showPresetMenu();

    /** Show a dialog to type a new preset name. */
    void showSaveAsDialog();

    /** Show a dialog to rename the current preset. */
    void showRenameDialog();

    PhuSplitterAudioProcessor& processorRef;
    PresetManager presetManager;

    juce::TextButton prevButton   { "<" };
    juce::TextButton nextButton   { ">" };
    juce::TextButton presetNameButton; // shows current preset name, opens dropdown
    juce::TextButton saveButton   { "Save" };
    juce::TextButton newButton    { "+New" };
    juce::TextButton initButton   { "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetStrip)
};
