#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../lib/SyncGlobals.h"
#include "LinkwitzRileyFilter.h"

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
    
    // 7-band multiband crossover for stereo to 7-channel splitting
    // 6 crossover frequencies create 7 bands
    static constexpr size_t NUM_BANDS = 7;
    static constexpr size_t NUM_CROSSOVER_FREQS = 6;
    
    // Default crossover frequencies for 7 bands (in Hz)
    // Sub bass, Bass, Low-mid, Mid, Upper-mid, Presence, Brilliance
    static constexpr std::array<float, NUM_CROSSOVER_FREQS> DEFAULT_CROSSOVER_FREQS = {
        80.0f,    // Sub bass / Bass split
        250.0f,   // Bass / Low-mid split
        500.0f,   // Low-mid / Mid split
        2000.0f,  // Mid / Upper-mid split
        6000.0f,  // Upper-mid / Presence split
        12000.0f  // Presence / Brilliance split
    };
    
    LinkwitzRiley::MultiBandN<float> m_multiBand;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuArpAudioProcessor)
};