#pragma once

#include "BandControlStrip.h"
#include "CrossoverFrequencyBar.h"
#include "PresetStrip.h"
#include <juce_audio_processors/juce_audio_processors.h>

class PhuSplitterAudioProcessor;

class PhuSplitterAudioProcessorEditor : public juce::AudioProcessorEditor {
  public:
    PhuSplitterAudioProcessorEditor(PhuSplitterAudioProcessor&);
    ~PhuSplitterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

#ifndef NDEBUG // Debug builds only
    // Add log message to debug window
    void addLogMessage(const juce::String& message);
#endif

  private:
    PhuSplitterAudioProcessor& audioProcessor;

    // Preset strip (Design 3: inline navigation)
    PresetStrip presetStrip;

    // Band control strip (solo/mute buttons)
    BandControlStrip bandControlStrip;

    // Crossover frequency bar
    CrossoverFrequencyBar crossoverBar;
    juce::Label crossoverLabel;

#ifndef NDEBUG // Debug builds only
    // Debug log text area (only in debug builds)
    juce::TextEditor logTextEditor;
    juce::Label logLabel;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuSplitterAudioProcessorEditor)
};