#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EditorLogger.h"
#include "../lib/EventSource.h"

PhuArpAudioProcessor::PhuArpAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::create7point0(), true))
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

    // Process stereo input through multiband crossover to 7 output channels
    const int numSamples = buffer.getNumSamples();
    const int numOutputChannels = buffer.getNumChannels();
    
    // Ensure we have stereo input (at minimum)
    if (numOutputChannels < 2)
    {
        syncGlobals.finishRun(numSamples);
        return;
    }
    
    // Get read pointers for input (first two channels are stereo input)
    const float* inputL = buffer.getReadPointer(0);
    const float* inputR = buffer.getReadPointer(1);
    
    // Process each sample through the multiband crossover
    // Output 7 bands to 7 channels
    std::array<float, NUM_BANDS> bandsL;
    std::array<float, NUM_BANDS> bandsR;
    
    const auto numOutputChannelsSize = static_cast<size_t>(numOutputChannels);
    
    for (int i = 0; i < numSamples; ++i)
    {
        // Process this sample through the multiband crossover
        m_multiBand.processSample(inputL[i], inputR[i], bandsL.data(), bandsR.data());
        
        // Write each band to corresponding output channel
        // Band index maps directly to channel index (0-6)
        for (size_t band = 0; band < NUM_BANDS && band < numOutputChannelsSize; ++band)
        {
            float* output = buffer.getWritePointer(static_cast<int>(band));
            // Sum stereo to mono for each band output
            output[i] = (bandsL[band] + bandsR[band]) * 0.5f;
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
    
    // Check if output is 7 channels (7.0)
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::create7point0())
        return false;
    
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