#pragma once

#include "../lib/audio/AudioSampleFifo.h"
#include "../lib/audio/FFTProcessor.h"
#include "../lib/audio/LinkwitzRileyFilter.h"
#include "../lib/events/SyncGlobals.h"
#include "../lib/network/CommandBroadcaster.h"
#include "../lib/network/SpectrumBroadcaster.h"
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

// Forward declarations
#ifndef NDEBUG // Debug builds only
namespace phu { namespace debug { class EditorLogger; } }
#endif

// Use namespaces
using phu::audio::AudioSampleFifo;
using phu::audio::FFTProcessor;
using phu::events::GlobalsEventListener;
using phu::network::CommandListener;
using phu::network::CommandBroadcaster;
using phu::network::SpectrumBroadcaster;
namespace LinkwitzRiley = phu::audio::LinkwitzRiley;

class PhuSplitterAudioProcessor : public juce::AudioProcessor,
                                  public GlobalsEventListener,
                                  public CommandListener,
                                  private juce::Timer {
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
    phu::debug::EditorLogger* getEditorLogger() const {
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

    // Get/set band gains (thread-safe)
    std::array<float, NUM_BANDS> getBandGains() const;
    void setBandGain(size_t bandIndex, float gainDB);

    // Parameter IDs for crossover frequencies
    static juce::String getCrossoverParamID(size_t index);

    // Parameter IDs for band gains
    static juce::String getBandGainParamID(size_t bandIndex);

    // Get/set solo state (thread-safe)
    std::array<bool, NUM_BANDS> getBandSoloStates() const;
    void setBandSolo(size_t bandIndex, bool solo);

    // Get/set mute state (thread-safe)
    std::array<bool, NUM_BANDS> getBandMuteStates() const;
    void setBandMute(size_t bandIndex, bool mute);

    // Parameter IDs for solo/mute
    static juce::String getBandSoloParamID(size_t bandIndex);
    static juce::String getBandMuteParamID(size_t bandIndex);

    // Lock-free FIFOs for UI spectrum display
    AudioSampleFifo<2>& getInputFifo() { return m_inputFifo; }
    AudioSampleFifo<2>& getOutputSumFifo() { return m_outputSumFifo; }

    // Spectrum broadcasting (owned by processor for headless operation)
    SpectrumBroadcaster& getSpectrumBroadcaster() { return m_spectrumBroadcaster; }
    bool isBroadcastEnabled() const { return m_broadcastEnabled.load(); }
    void setBroadcastEnabled(bool enabled);

    // Command broadcasting (owned by processor alongside spectrum broadcaster)
    CommandBroadcaster& getCommandBroadcaster() { return m_commandBroadcaster; }

    /** Broadcast a solo state change to all peers (called from Alt+Click). */
    void broadcastSoloCommand(size_t bandIndex, bool solo);

    /** Broadcast a mute state change to all peers (called from Alt+Click). */
    void broadcastMuteCommand(size_t bandIndex, bool mute);

    // CommandListener interface
    void onCommandReceived(phu::network::CommandType commandType,
                           uint32_t senderID,
                           const std::string& targetGroup,
                           const uint8_t* payload,
                           uint16_t payloadSize) override;

  private:
    // Timer callback drives broadcast FFT + spectrum sending (runs even when editor is closed)
    void timerCallback() override;
    // Create parameter layout for APVTS
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    // DAW synchronization globals (each instance has its own)
    phu::events::SyncGlobals syncGlobals;

#ifndef NDEBUG // Debug builds only
    // Logger for editor log view (debug builds only)
    std::unique_ptr<phu::debug::EditorLogger> editorLogger;
#endif

    // APVTS for DAW parameter automation & state save/restore
    juce::AudioProcessorValueTreeState apvts;

    // Cached atomic pointers to crossover frequency parameters (for audio thread)
    std::array<std::atomic<float>*, NUM_CROSSOVER_FREQS> crossoverParamPtrs{};

    // Current frequencies used by audio thread (updated from params each block)
    std::array<float, NUM_CROSSOVER_FREQS> currentFreqs = DEFAULT_CROSSOVER_FREQS;

    // Cached atomic pointers to band gain parameters (for audio thread)
    std::array<std::atomic<float>*, NUM_BANDS> bandGainParamPtrs{};

    // Current band gains used by audio thread (updated from params each block)
    std::array<float, NUM_BANDS> currentGainsDB{};

    // Precomputed linear gains (updated when gains change)
    std::array<float, NUM_BANDS> currentLinearGains{};

    // Cached atomic pointers to solo/mute parameters (for audio thread)
    std::array<std::atomic<float>*, NUM_BANDS> bandSoloParamPtrs{};
    std::array<std::atomic<float>*, NUM_BANDS> bandMuteParamPtrs{};

    LinkwitzRiley::MultiBandN<float> m_multiBand;

    // Lock-free FIFOs for transferring audio samples to UI thread (spectrum display)
    AudioSampleFifo<2> m_inputFifo;
    AudioSampleFifo<2> m_outputSumFifo;

    // Spectrum broadcasting (lives in processor so broadcast continues when editor is closed)
    SpectrumBroadcaster m_spectrumBroadcaster;
    FFTProcessor m_broadcastFFT{12};            // Dedicated FFT for broadcast
    AudioSampleFifo<2> m_broadcastFifo;          // Dedicated FIFO fed from processBlock
    std::atomic<bool> m_broadcastEnabled{false}; // Persisted in state

    // Command broadcasting (lives in processor alongside spectrum broadcaster)
    CommandBroadcaster m_commandBroadcaster;

    // Temp buffer for accumulating output sum per processBlock
    // Max expected host buffer size; if larger, we process in chunks
    static constexpr int kMaxBlockSize = 8192;
    std::array<float, kMaxBlockSize> m_sumL{};
    std::array<float, kMaxBlockSize> m_sumR{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuSplitterAudioProcessor)
};