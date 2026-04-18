#include "PluginProcessor.h"
#include "PluginEditor.h"
#if PHU_DEBUG_UI // Debug builds only
#include "debug/EditorLogger.h"
using phu::debug::EditorLogger;
#endif
#include "events/EventSource.h"

using namespace phu::events;
using namespace phu::audio::LinkwitzRiley;
using phu::network::CommandType;
using phu::network::SoloMutePayload;

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
#if PHU_DEBUG_UI // Debug builds only
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

    // Cache raw parameter pointers for solo/mute
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        bandSoloParamPtrs[i] = apvts.getRawParameterValue(getBandSoloParamID(i));
        bandMuteParamPtrs[i] = apvts.getRawParameterValue(getBandMuteParamID(i));
    }

    // Initialize multiband crossover with default frequencies
    m_multiBand.initialize(LinkwitzRiley::Slope::DB48, DEFAULT_CROSSOVER_FREQS.data(),
                           NUM_CROSSOVER_FREQS, 44100.0f);

#if PHU_DEBUG_UI // Debug builds only
    LOG_MESSAGE(editorLogger.get(), "Audio processing plugin initialized");
#endif
}

PhuSplitterAudioProcessor::~PhuSplitterAudioProcessor() {
    // Stop broadcast timer and shutdown broadcasters
    stopTimer();
    m_commandBroadcaster.removeListener(this);
    m_commandBroadcaster.shutdown();
    m_spectrumBroadcaster.shutdown();
}

void PhuSplitterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    (void)samplesPerBlock; // Unused parameter
    syncGlobals.updateSampleRate(sampleRate);

    // Reset spectrum display FIFOs on playback restart / sample rate change
    m_inputFifo.reset();
    m_outputSumFifo.reset();
    m_broadcastFifo.reset();

    // Reset per-band waveform FIFOs
    for (auto& f : m_bandPreGainFifos) f.reset();
    for (auto& f : m_bandPostGainFifos) f.reset();

    // Initialize broadcasters (one-time setup, stays running)
    if (!m_spectrumBroadcaster.isRunning()) {
        if (m_spectrumBroadcaster.initialize()) {
            m_spectrumBroadcaster.setReceiveEnabled(m_receiveEnabled.load());
            m_spectrumBroadcaster.setBroadcastEnabled(m_broadcastEnabled.load());
        }
    }
    if (!m_commandBroadcaster.isRunning()) {
        if (m_commandBroadcaster.initialize()) {
            m_commandBroadcaster.addListener(this);
        }
    }

    // Start timer if either broadcast or receive is enabled
    if ((m_broadcastEnabled.load() || m_receiveEnabled.load()) && !isTimerRunning()) {
        startTimerHz(30); // 30 Hz for FFT processing
    }

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

#if PHU_DEBUG_UI // Debug builds only
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

    // Push raw stereo input into FIFO for UI spectrum display
    {
        const float* inputChannels[2] = { inputL, inputR };
        m_inputFifo.push(inputChannels, numSamples);
    }

    // Process each sample through the multiband crossover
    // Output 7 stereo bands (14 channels total)
    std::array<float, NUM_BANDS> bandsL;
    std::array<float, NUM_BANDS> bandsR;

    // Check if any band is soloed
    bool anySolo = false;
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        if (bandSoloParamPtrs[i]->load() > 0.5f) {
            anySolo = true;
            break;
        }
    }

    // Determine which bands should play based on solo/mute logic
    // Precompute final gains for each band (once per block, not per sample)
    std::array<float, NUM_BANDS> finalGains;
    for (size_t band = 0; band < NUM_BANDS; ++band) {
        bool shouldPlay;
        if (anySolo) {
            // Solo mode: only soloed bands play
            shouldPlay = (bandSoloParamPtrs[band]->load() > 0.5f);
        } else {
            // Normal mode: muted bands don't play
            shouldPlay = !(bandMuteParamPtrs[band]->load() > 0.5f);
        }
        finalGains[band] = shouldPlay ? currentLinearGains[band] : 0.0f;
    }

    // Clear sum buffers for output sum accumulation
    const int samplesToProcess = juce::jmin(numSamples, kMaxBlockSize);
    std::memset(m_sumL.data(), 0, sizeof(float) * static_cast<size_t>(samplesToProcess));
    std::memset(m_sumR.data(), 0, sizeof(float) * static_cast<size_t>(samplesToProcess));

    for (int i = 0; i < numSamples; ++i) {
        // Process this sample through the multiband crossover
        m_multiBand.processSample(inputL[i], inputR[i], bandsL.data(), bandsR.data());

        // Apply band gains and solo/mute logic, then write each band to corresponding stereo output channel pair
        // Band 0 -> channels 0,1 (after input); Band 1 -> channels 2,3; etc.
        for (size_t band = 0; band < NUM_BANDS; ++band) {
            // Store pre-gain samples for waveform display FIFO
            if (i < kMaxBlockSize) {
                m_preBandL[band][static_cast<size_t>(i)] = bandsL[band];
                m_preBandR[band][static_cast<size_t>(i)] = bandsR[band];
            }

            const int leftChannel = static_cast<int>(band * 2);
            const int rightChannel = leftChannel + 1;

            const float bandL = bandsL[band] * finalGains[band];
            const float bandR = bandsR[band] * finalGains[band];

            if (leftChannel < totalOutputChannels) {
                float* outL = buffer.getWritePointer(leftChannel);
                outL[i] = bandL;
            }
            if (rightChannel < totalOutputChannels) {
                float* outR = buffer.getWritePointer(rightChannel);
                outR[i] = bandR;
            }

            // Accumulate output sum for spectrum display
            if (i < kMaxBlockSize) {
                m_sumL[static_cast<size_t>(i)] += bandL;
                m_sumR[static_cast<size_t>(i)] += bandR;
            }
        }
    }

    // Push stereo output sum into FIFO for UI spectrum display
    {
        const float* sumChannels[2] = { m_sumL.data(), m_sumR.data() };
        m_outputSumFifo.push(sumChannels, samplesToProcess);
    }

    // Push stereo output sum into broadcast FIFO (for headless broadcast FFT)
    if (m_broadcastEnabled.load()) {
        const float* sumChannels[2] = { m_sumL.data(), m_sumR.data() };
        m_broadcastFifo.push(sumChannels, samplesToProcess);
    }

    // Push per-band samples to FIFOs for rolling waveform display
    for (size_t band = 0; band < NUM_BANDS; ++band) {
        // Pre-gain FIFO (from block buffers filled in sample loop)
        const float* preChannels[2] = { m_preBandL[band].data(), m_preBandR[band].data() };
        m_bandPreGainFifos[band].push(preChannels, samplesToProcess);

        // Post-gain FIFO (read directly from output bus buffer)
        const int leftCh = static_cast<int>(band * 2);
        const int rightCh = leftCh + 1;
        if (leftCh < totalOutputChannels && rightCh < totalOutputChannels) {
            const float* postChannels[2] = { buffer.getReadPointer(leftCh),
                                              buffer.getReadPointer(rightCh) };
            m_bandPostGainFifos[band].push(postChannels, samplesToProcess);
        }
    }

    // Mark end of processing
    syncGlobals.finishRun(buffer.getNumSamples());
}

juce::AudioProcessorEditor* PhuSplitterAudioProcessor::createEditor() {
    auto* editor = new PhuSplitterAudioProcessorEditor(*this);

#if PHU_DEBUG_UI // Debug builds only
    // Register editor as log sink
    if (editorLogger) {
        editorLogger->setSink(editor);
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
    // Persist broadcast enabled state as attribute on root element
    xml->setAttribute("broadcastEnabled", m_broadcastEnabled.load());
    copyXmlToBinary(*xml, destData);
}

void PhuSplitterAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        // Restore broadcast enabled state
        bool wasBroadcasting = xml->getBoolAttribute("broadcastEnabled", false);
        if (wasBroadcasting) {
            setBroadcastEnabled(true);
        }
    }
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

juce::String PhuSplitterAudioProcessor::getBandSoloParamID(size_t bandIndex) {
    return "band" + juce::String(static_cast<int>(bandIndex)) + "_solo";
}

juce::String PhuSplitterAudioProcessor::getBandMuteParamID(size_t bandIndex) {
    return "band" + juce::String(static_cast<int>(bandIndex)) + "_mute";
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

    // Add solo parameters (boolean)
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        auto paramID = getBandSoloParamID(i);
        auto name = gainBandNames[static_cast<int>(i)] + " Solo";

        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{paramID, 1}, name, false));
    }

    // Add mute parameters (boolean)
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        auto paramID = getBandMuteParamID(i);
        auto name = gainBandNames[static_cast<int>(i)] + " Mute";

        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{paramID, 1}, name, false));
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

std::array<bool, PhuSplitterAudioProcessor::NUM_BANDS>
PhuSplitterAudioProcessor::getBandSoloStates() const {
    std::array<bool, NUM_BANDS> states;
    for (size_t i = 0; i < NUM_BANDS; ++i)
        states[i] = (bandSoloParamPtrs[i]->load() > 0.5f);
    return states;
}

void PhuSplitterAudioProcessor::setBandSolo(size_t bandIndex, bool solo) {
    if (bandIndex < NUM_BANDS) {
        auto* param = apvts.getParameter(getBandSoloParamID(bandIndex));
        if (param)
            param->setValueNotifyingHost(solo ? 1.0f : 0.0f);
    }
}

std::array<bool, PhuSplitterAudioProcessor::NUM_BANDS>
PhuSplitterAudioProcessor::getBandMuteStates() const {
    std::array<bool, NUM_BANDS> states;
    for (size_t i = 0; i < NUM_BANDS; ++i)
        states[i] = (bandMuteParamPtrs[i]->load() > 0.5f);
    return states;
}

void PhuSplitterAudioProcessor::setBandMute(size_t bandIndex, bool mute) {
    if (bandIndex < NUM_BANDS) {
        auto* param = apvts.getParameter(getBandMuteParamID(bandIndex));
        if (param)
            param->setValueNotifyingHost(mute ? 1.0f : 0.0f);
    }
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PhuSplitterAudioProcessor();
}

// ============================================================================
// Spectrum broadcasting
// ============================================================================

void PhuSplitterAudioProcessor::setBroadcastEnabled(bool enabled) {
    if (enabled == m_broadcastEnabled.load())
        return;

    m_broadcastEnabled.store(enabled);

    if (enabled) {
        m_broadcastFifo.reset();
        // Enable broadcasting in the broadcaster
        if (m_spectrumBroadcaster.isRunning()) {
            m_spectrumBroadcaster.setBroadcastEnabled(true);
        }
        // Start timer if not already running (needed for FFT processing)
        if (!isTimerRunning()) {
            startTimerHz(30);
        }
    } else {
        // Disable broadcasting but keep receiving if enabled
        m_spectrumBroadcaster.setBroadcastEnabled(false);
        // Stop timer only if receive is also disabled
        if (!m_receiveEnabled.load() && isTimerRunning()) {
            stopTimer();
        }
    }
}

void PhuSplitterAudioProcessor::setReceiveEnabled(bool enabled) {
    if (enabled == m_receiveEnabled.load())
        return;

    m_receiveEnabled.store(enabled);

    if (enabled) {
        // Enable receiving in the broadcaster
        if (m_spectrumBroadcaster.isRunning()) {
            m_spectrumBroadcaster.setReceiveEnabled(true);
        }
        // Start timer if not already running (needed for FFT processing even if just receiving)
        if (!isTimerRunning()) {
            startTimerHz(30);
        }
    } else {
        // Disable receiving but keep broadcasting if enabled
        m_spectrumBroadcaster.setReceiveEnabled(false);
        // Stop timer only if broadcast is also disabled
        if (!m_broadcastEnabled.load() && isTimerRunning()) {
            stopTimer();
        }
    }
}

// ============================================================================
// Command broadcasting
// ============================================================================

void PhuSplitterAudioProcessor::broadcastSoloCommand(size_t bandIndex, bool solo) {
    if (bandIndex >= NUM_BANDS)
        return;
    m_commandBroadcaster.sendSoloCommand(static_cast<uint8_t>(bandIndex), solo);
}

void PhuSplitterAudioProcessor::broadcastMuteCommand(size_t bandIndex, bool mute) {
    if (bandIndex >= NUM_BANDS)
        return;
    m_commandBroadcaster.sendMuteCommand(static_cast<uint8_t>(bandIndex), mute);
}

void PhuSplitterAudioProcessor::onCommandReceived(CommandType commandType,
                                                  uint32_t /*senderID*/,
                                                  const std::string& /*targetGroup*/,
                                                  const uint8_t* payload,
                                                  uint16_t payloadSize) {
    // Dispatch received commands — called on receiver background thread.
    // setBandSolo/setBandMute use setValueNotifyingHost which is thread-safe.
    switch (commandType) {
        case CommandType::Solo: {
            if (payloadSize >= sizeof(SoloMutePayload)) {
                auto p = reinterpret_cast<const SoloMutePayload*>(payload);
                if (p->bandIndex < NUM_BANDS)
                    setBandSolo(p->bandIndex, p->state != 0);
            }
            break;
        }
        case CommandType::Mute: {
            if (payloadSize >= sizeof(SoloMutePayload)) {
                auto p = reinterpret_cast<const SoloMutePayload*>(payload);
                if (p->bandIndex < NUM_BANDS)
                    setBandMute(p->bandIndex, p->state != 0);
            }
            break;
        }
        default:
            break; // Unknown command — ignore
    }
}

void PhuSplitterAudioProcessor::timerCallback() {
    // Process broadcast FFT from dedicated FIFO (always process for local visualization)
    m_broadcastFFT.process(m_broadcastFifo);

    // Broadcast computed spectrum only if broadcasting is enabled
    if (m_broadcastEnabled.load()) {
        const float* magnitudes = m_broadcastFFT.getMagnitudeSpectrum();
        int numBins = m_broadcastFFT.getNumBins();
        float sampleRate = static_cast<float>(getSampleRate());

        if (numBins > 0 && sampleRate > 0.0f) {
            m_spectrumBroadcaster.broadcastSpectrum(magnitudes, numBins, sampleRate);
        }
    }
}