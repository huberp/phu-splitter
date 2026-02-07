#pragma once

#include "../lib/SyncGlobals.h"
#include "LinkwitzRileyFilter.h"
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

#ifndef NDEBUG // Debug builds only
class EditorLogger;
#endif

class PhuSplitterAudioProcessor : public juce::AudioProcessor, public GlobalsEventListener {
  public:
    PhuSplitterAudioProcessor();
    ~PhuSplitterAudioProcessor() override;

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

#ifndef NDEBUG // Debug builds only
    // Get the editor logger (for editor registration)
    EditorLogger* getEditorLogger() const {
        return editorLogger.get();
    }
#endif

    // Constants
    static constexpr size_t NUM_BANDS = 7;
    static constexpr size_t NUM_CROSSOVER_FREQS = 6;

    // Default crossover frequencies for 7 bands (in Hz)
    static constexpr std::array<float, NUM_CROSSOVER_FREQS> DEFAULT_CROSSOVER_FREQS = {
        80.0f,   // Sub bass / Bass split
        250.0f,  // Bass / Low-mid split
        500.0f,  // Low-mid / Mid split
        2000.0f, // Mid / Upper-mid split
        6000.0f, // Upper-mid / Presence split
        12000.0f // Presence / Brilliance split
    };

    // Parameter tree state for automatable parameters
    juce::AudioProcessorValueTreeState& getAPVTS() {
        return apvts;
    }

    // Get/set crossover frequencies (thread-safe)
    std::array<float, NUM_CROSSOVER_FREQS> getCrossoverFrequencies() const;
    void setCrossoverFrequency(size_t index, float freqHz);

    // Parameter IDs for crossover frequencies
    static juce::String getCrossoverParamID(size_t index);

  private:
    // Create parameter layout for APVTS
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    // DAW synchronization globals (each instance has its own)
    SyncGlobals syncGlobals;

#ifndef NDEBUG // Debug builds only
    // Logger for editor log view (debug builds only)
    std::unique_ptr<EditorLogger> editorLogger;
#endif

    // APVTS for DAW parameter automation & state save/restore
    juce::AudioProcessorValueTreeState apvts;

    // Cached atomic pointers to crossover frequency parameters (for audio thread)
    std::array<std::atomic<float>*, NUM_CROSSOVER_FREQS> crossoverParamPtrs{};

    // Current frequencies used by audio thread (updated from params each block)
    std::array<float, NUM_CROSSOVER_FREQS> currentFreqs = DEFAULT_CROSSOVER_FREQS;

    LinkwitzRiley::MultiBandN<float> m_multiBand;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuSplitterAudioProcessor)
};