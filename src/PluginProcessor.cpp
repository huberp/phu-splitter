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
    // Log initialization
    LOG_MESSAGE(editorLogger.get(), "Audio processing plugin initialized");
}

PhuArpAudioProcessor::~PhuArpAudioProcessor() 
{
}

void PhuArpAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    syncGlobals.updateSampleRate(sampleRate);

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

    // Basic audio processing - for now just pass through
    // The audio is already in the buffer, so we don't need to do anything for passthrough
    
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