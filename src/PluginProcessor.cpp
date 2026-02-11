#include "PluginProcessor.h"
#include "PluginEditor.h"
#ifndef NDEBUG // Debug builds only
#include "EditorLogger.h"
#endif
#include "../lib/EventSource.h"

PhuSplitterAudioProcessor::PhuSplitterAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Band 1", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Band 2", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Band 3", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Band 4", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Band 5", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Band 6", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Band 7", juce::AudioChannelSet::stereo(), true))
#ifndef NDEBUG // Debug builds only
      ,
      editorLogger(std::make_unique<EditorLogger>())
#endif
      ,
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {
    // Cache raw parameter pointers for audio-thread access
    for (size_t i = 0; i < NUM_CROSSOVER_FREQS; ++i) {
        crossoverParamPtrs[i] = apvts.getRawParameterValue(getCrossoverParamID(i));
    }

    // Cache raw parameter pointers for band gains
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        bandGainParamPtrs[i] = apvts.getRawParameterValue(getBandGainParamID(i));
    }

    // Initialize multiband crossover with default frequencies
    m_multiBand.initialize(LinkwitzRiley::Slope::DB48, DEFAULT_CROSSOVER_FREQS.data(),
                           NUM_CROSSOVER_FREQS, 44100.0f);

#ifndef NDEBUG // Debug builds only
    LOG_MESSAGE(editorLogger.get(), "Audio processing plugin initialized");
#endif
}

PhuSplitterAudioProcessor::~PhuSplitterAudioProcessor() {
}

void PhuSplitterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    syncGlobals.updateSampleRate(sampleRate);

    // Read current crossover frequencies from parameters
    for (size_t i = 0; i < NUM_CROSSOVER_FREQS; ++i)
        currentFreqs[i] = crossoverParamPtrs[i]->load();

    // Read current band gains from parameters
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        currentGainsDB[i] = bandGainParamPtrs[i]->load();
        currentLinearGains[i] = std::pow(10.0f, currentGainsDB[i] / 20.0f);
    }

    // Configure multiband crossover with actual sample rate
    m_multiBand.setParams(LinkwitzRiley::Slope::DB48, currentFreqs.data(), NUM_CROSSOVER_FREQS,
                          static_cast<float>(sampleRate));
    m_multiBand.reset();

#ifndef NDEBUG // Debug builds only
    if (editorLogger)
        editorLogger->markCurrentThreadAsAudioThread();
#endif
}

void PhuSplitterAudioProcessor::releaseResources() {
}

void PhuSplitterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    // Check if crossover frequencies changed from parameter automation
    {
        bool changed = false;
        for (size_t i = 0; i < NUM_CROSSOVER_FREQS; ++i) {
            float paramVal = crossoverParamPtrs[i]->load();
            if (paramVal != currentFreqs[i]) {
                currentFreqs[i] = paramVal;
                changed = true;
            }
        }
        if (changed) {
            m_multiBand.setParams(LinkwitzRiley::Slope::DB48, currentFreqs.data(),
                                  NUM_CROSSOVER_FREQS, static_cast<float>(getSampleRate()));
        }
    }

    // Check if band gains changed from parameter automation and precompute linear gains
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        float paramVal = bandGainParamPtrs[i]->load();
        if (paramVal != currentGainsDB[i]) {
            currentGainsDB[i] = paramVal;
            // Precompute linear gain to avoid expensive pow() in sample loop
            currentLinearGains[i] = std::pow(10.0f, currentGainsDB[i] / 20.0f);
        }
    }

    // Get playhead position info
    auto playHeadPtr = getPlayHead();
    auto positionInfo = playHeadPtr ? playHeadPtr->getPosition()
                                    : juce::Optional<juce::AudioPlayHead::PositionInfo>();

    // Update DAW globals
    syncGlobals.updateDAWGlobals(buffer, midiMessages, positionInfo);

#ifndef NDEBUG // Debug builds only
    // Test logging (can be removed later)
    const auto currentRun = syncGlobals.getCurrentRun();
    if (currentRun % 1000 == 0) {
        LOG_MESSAGE(editorLogger.get(), "Processed " + juce::String(currentRun) + " audio blocks");
    }
#endif

    // Process stereo input through multiband crossover to 7 stereo output channels
    const int numSamples = buffer.getNumSamples();
    const int totalOutputChannels = getTotalNumOutputChannels();

    // We need at least stereo input
    if (getTotalNumInputChannels() < 2) {
        syncGlobals.finishRun(numSamples);
        return;
    }

    // Get read pointers for stereo input (first two channels)
    const float* inputL = buffer.getReadPointer(0);
    const float* inputR = buffer.getReadPointer(1);

    // Process each sample through the multiband crossover
    // Output 7 stereo bands (14 channels total)
    std::array<float, NUM_BANDS> bandsL;
    std::array<float, NUM_BANDS> bandsR;

    for (int i = 0; i < numSamples; ++i) {
        // Process this sample through the multiband crossover
        m_multiBand.processSample(inputL[i], inputR[i], bandsL.data(), bandsR.data());

        // Apply band gains and write each band to corresponding stereo output channel pair
        // Band 0 -> channels 0,1 (after input); Band 1 -> channels 2,3; etc.
        for (size_t band = 0; band < NUM_BANDS; ++band) {
            const int leftChannel = static_cast<int>(band * 2);
            const int rightChannel = leftChannel + 1;

            if (leftChannel < totalOutputChannels) {
                float* outL = buffer.getWritePointer(leftChannel);
                outL[i] = bandsL[band] * currentLinearGains[band];
            }
            if (rightChannel < totalOutputChannels) {
                float* outR = buffer.getWritePointer(rightChannel);
                outR[i] = bandsR[band] * currentLinearGains[band];
            }
        }
    }

    // Mark end of processing
    syncGlobals.finishRun(buffer.getNumSamples());
}

juce::AudioProcessorEditor* PhuSplitterAudioProcessor::createEditor() {
    auto* editor = new PhuSplitterAudioProcessorEditor(*this);

#ifndef NDEBUG // Debug builds only
    // Register editor with logger
    if (editorLogger) {
        editorLogger->setEditor(editor);
        LOG_MESSAGE(editorLogger.get(), "Editor opened");
    }
#endif

    return editor;
}

bool PhuSplitterAudioProcessor::hasEditor() const {
    return true;
}

const juce::String PhuSplitterAudioProcessor::getName() const {
    return "PhuSplitter";
}
bool PhuSplitterAudioProcessor::acceptsMidi() const {
    return false;
}
bool PhuSplitterAudioProcessor::producesMidi() const {
    return false;
}
bool PhuSplitterAudioProcessor::isMidiEffect() const {
    return false;
}
double PhuSplitterAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

bool PhuSplitterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // Check if input is stereo
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Check that we have 7 stereo output buses
    if (layouts.outputBuses.size() != NUM_BANDS)
        return false;

    // Each output bus must be stereo
    for (const auto& bus : layouts.outputBuses) {
        if (bus != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

int PhuSplitterAudioProcessor::getNumPrograms() {
    return 1;
}
int PhuSplitterAudioProcessor::getCurrentProgram() {
    return 0;
}
void PhuSplitterAudioProcessor::setCurrentProgram(int) {
}
const juce::String PhuSplitterAudioProcessor::getProgramName(int) {
    return "Default";
}
void PhuSplitterAudioProcessor::changeProgramName(int, const juce::String&) {
}

void PhuSplitterAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PhuSplitterAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// ============================================================================
// Parameter helpers
// ============================================================================

juce::String PhuSplitterAudioProcessor::getCrossoverParamID(size_t index) {
    return "crossover_freq_" + juce::String(static_cast<int>(index + 1));
}

juce::String PhuSplitterAudioProcessor::getBandGainParamID(size_t bandIndex) {
    return "band" + juce::String(static_cast<int>(bandIndex)) + "_gain";
}

juce::AudioProcessorValueTreeState::ParameterLayout
PhuSplitterAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    static const juce::StringArray bandLabels = {"Sub/Bass",  "Bass/LoMid", "LoMid/Mid",
                                                 "Mid/HiMid", "HiMid/Pres", "Pres/Brill"};

    for (size_t i = 0; i < NUM_CROSSOVER_FREQS; ++i) {
        auto paramID = getCrossoverParamID(i);
        auto name = "XOver " + bandLabels[static_cast<int>(i)];

        // Log-skewed range: 20 Hz .. 20000 Hz, skew ~0.25 for log-like distribution
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{paramID, 1}, name,
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.25f),
            DEFAULT_CROSSOVER_FREQS[i]));
    }

    // Add band gain parameters
    static const juce::StringArray gainBandNames = {"SUB",    "BASS", "LO-MID", "MID",
                                                    "HI-MID", "PRES", "BRILL"};

    for (size_t i = 0; i < NUM_BANDS; ++i) {
        auto paramID = getBandGainParamID(i);
        auto name = gainBandNames[static_cast<int>(i)] + " Gain";

        // Linear range: -24.0 dB to +24.0 dB
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{paramID, 1}, name,
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    }

    return layout;
}

std::array<float, PhuSplitterAudioProcessor::NUM_CROSSOVER_FREQS>
PhuSplitterAudioProcessor::getCrossoverFrequencies() const {
    std::array<float, NUM_CROSSOVER_FREQS> freqs;
    for (size_t i = 0; i < NUM_CROSSOVER_FREQS; ++i)
        freqs[i] = crossoverParamPtrs[i]->load();
    return freqs;
}

void PhuSplitterAudioProcessor::setCrossoverFrequency(size_t index, float freqHz) {
    if (index < NUM_CROSSOVER_FREQS) {
        auto* param = apvts.getParameter(getCrossoverParamID(index));
        if (param)
            param->setValueNotifyingHost(param->convertTo0to1(freqHz));
    }
}

std::array<float, PhuSplitterAudioProcessor::NUM_BANDS>
PhuSplitterAudioProcessor::getBandGains() const {
    std::array<float, NUM_BANDS> gains;
    for (size_t i = 0; i < NUM_BANDS; ++i)
        gains[i] = bandGainParamPtrs[i]->load();
    return gains;
}

void PhuSplitterAudioProcessor::setBandGain(size_t bandIndex, float gainDB) {
    if (bandIndex < NUM_BANDS) {
        auto* param = apvts.getParameter(getBandGainParamID(bandIndex));
        if (param)
            param->setValueNotifyingHost(param->convertTo0to1(gainDB));
    }
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PhuSplitterAudioProcessor();
}