#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "..\lib\SyncGlobals.h"

class EditorLogger;

class PhuArpAudioProcessor : public juce::AudioProcessor,
                               public GlobalsEventListener
{
public:
    PhuArpAudioProcessor();
    ~PhuArpAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    
    // Get the editor logger (for editor registration)
    EditorLogger* getEditorLogger() const { return editorLogger.get(); }

private:
    // DAW synchronization globals (each instance has its own)
    SyncGlobals syncGlobals;
    
    // Logger for editor log view
    std::unique_ptr<EditorLogger> editorLogger;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuArpAudioProcessor)
};