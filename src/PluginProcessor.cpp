#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EditorLogger.h"
#include "../lib/EventSource.h"

PhuArpAudioProcessor::PhuArpAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Band 1", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Band 2", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Band 3", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Band 4", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Band 5", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Band 6", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Band 7", juce::AudioChannelSet::stereo(), true))
    , editorLogger(std::make_unique<EditorLogger>())
{
    // Initialize multiband crossover with default frequencies (will be properly configured in prepareToPlay)
    m_multiBand.initialize(LinkwitzRiley::Slope::DB24, DEFAULT_CROSSOVER_FREQS.data(), 
                           NUM_CROSSOVER_FREQS, 44100.0f);
    
    // Log initialization
    LOG_MESSAGE(editorLogger.get(), "Audio processing plugin initialized");
}

PhuArpAudioProcessor::~PhuArpAudioProcessor() 
{
}

void PhuArpAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    syncGlobals.updateSampleRate(sampleRate);
    
    // Configure multiband crossover with actual sample rate
    m_multiBand.setParams(LinkwitzRiley::Slope::DB24, DEFAULT_CROSSOVER_FREQS.data(),
                          NUM_CROSSOVER_FREQS, static_cast<float>(sampleRate));
    m_multiBand.reset();

    // Mark the current thread as the audio thread for realtime-safe logging.
    if (editorLogger)
        editorLogger->markCurrentThreadAsAudioThread();
}

void PhuArpAudioProcessor::releaseResources() {}

void PhuArpAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    // Get playhead position info
    auto playHeadPtr = getPlayHead();
    auto positionInfo = playHeadPtr ? playHeadPtr->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    
    // Update DAW globals
    syncGlobals.updateDAWGlobals(
        buffer,
        midiMessages,
        positionInfo
    );
    
    // Test logging (can be removed later)
    const auto currentRun = syncGlobals.getCurrentRun();
    if (currentRun % 1000 == 0)
    {
        LOG_MESSAGE(editorLogger.get(), "Processed " + juce::String(currentRun) + " audio blocks");
    }

    // Process stereo input through multiband crossover to 7 stereo output channels
    const int numSamples = buffer.getNumSamples();
    const int totalOutputChannels = getTotalNumOutputChannels();
    
    // We need at least stereo input
    if (getTotalNumInputChannels() < 2)
    {
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
    
    for (int i = 0; i < numSamples; ++i)
    {
        // Process this sample through the multiband crossover
        m_multiBand.processSample(inputL[i], inputR[i], bandsL.data(), bandsR.data());
        
        // Write each band to corresponding stereo output channel pair
        // Band 0 -> channels 0,1 (after input); Band 1 -> channels 2,3; etc.
        for (size_t band = 0; band < NUM_BANDS; ++band)
        {
            const int leftChannel = static_cast<int>(band * 2);
            const int rightChannel = leftChannel + 1;
            
            if (leftChannel < totalOutputChannels)
            {
                float* outL = buffer.getWritePointer(leftChannel);
                outL[i] = bandsL[band];
            }
            if (rightChannel < totalOutputChannels)
            {
                float* outR = buffer.getWritePointer(rightChannel);
                outR[i] = bandsR[band];
            }
        }
    }
    
    // Mark end of processing
    syncGlobals.finishRun(buffer.getNumSamples());
}

juce::AudioProcessorEditor* PhuArpAudioProcessor::createEditor() 
{ 
    auto* editor = new PhuArpAudioProcessorEditor(*this);
    
    // Register editor with logger
    if (editorLogger)
    {
        editorLogger->setEditor(editor);
        LOG_MESSAGE(editorLogger.get(), "Editor opened");
    }
    
    return editor;
}

bool PhuArpAudioProcessor::hasEditor() const { return true; }

const juce::String PhuArpAudioProcessor::getName() const { return "PhuArp"; }
bool PhuArpAudioProcessor::acceptsMidi() const { return false; }
bool PhuArpAudioProcessor::producesMidi() const { return false; }
bool PhuArpAudioProcessor::isMidiEffect() const { return false; }
double PhuArpAudioProcessor::getTailLengthSeconds() const { return 0.0; }

bool PhuArpAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Check if input is stereo
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    
    // Check that we have 7 stereo output buses
    if (layouts.outputBuses.size() != NUM_BANDS)
        return false;
    
    // Each output bus must be stereo
    for (const auto& bus : layouts.outputBuses)
    {
        if (bus != juce::AudioChannelSet::stereo())
            return false;
    }
    
    return true;
}

int PhuArpAudioProcessor::getNumPrograms() { return 1; }
int PhuArpAudioProcessor::getCurrentProgram() { return 0; }
void PhuArpAudioProcessor::setCurrentProgram(int) {}
const juce::String PhuArpAudioProcessor::getProgramName(int) { return "Default"; }
void PhuArpAudioProcessor::changeProgramName(int, const juce::String&) {}

void PhuArpAudioProcessor::getStateInformation(juce::MemoryBlock&) {}
void PhuArpAudioProcessor::setStateInformation(const void*, int) {}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhuArpAudioProcessor();
}