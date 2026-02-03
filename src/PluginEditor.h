#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PhuSplitterAudioProcessor;

class PhuSplitterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    PhuSplitterAudioProcessorEditor(PhuSplitterAudioProcessor&);
    ~PhuSplitterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    
    // Add log message to debug window
    void addLogMessage(const juce::String& message);

private:
    PhuSplitterAudioProcessor& audioProcessor;

    // Debug log text area
    juce::TextEditor logTextEditor;
    juce::Label logLabel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuSplitterAudioProcessorEditor)
};