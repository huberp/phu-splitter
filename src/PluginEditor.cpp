#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "EditorLogger.h"

PhuArpAudioProcessorEditor::PhuArpAudioProcessorEditor(PhuArpAudioProcessor& p) 
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Set up debug log label
    logLabel.setText("Debug Log", juce::dontSendNotification);
    logLabel.setJustificationType(juce::Justification::centredLeft);
    logLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(logLabel);
    
    // Set up debug log text editor
    logTextEditor.setMultiLine(true);
    logTextEditor.setReadOnly(true);
    logTextEditor.setScrollbarsShown(true);
    logTextEditor.setCaretVisible(false);
    logTextEditor.setPopupMenuEnabled(true);
    logTextEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    logTextEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    logTextEditor.setColour(juce::TextEditor::textColourId, juce::Colours::lightgreen);
    logTextEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::grey);
    addAndMakeVisible(logTextEditor);
    
    // Set editor size
    setSize(600, 400);
    
    // Add initial welcome message
    addLogMessage("PhuArp Debug Log initialized");
}

PhuArpAudioProcessorEditor::~PhuArpAudioProcessorEditor() 
{
    // Unregister from logger
    if (auto* logger = audioProcessor.getEditorLogger())
    {
        logger->clearEditor();
    }
}

void PhuArpAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PhuArpAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    // Label at top
    logLabel.setBounds(area.removeFromTop(25));
    area.removeFromTop(5); // Spacing
    
    // Text editor takes remaining space
    logTextEditor.setBounds(area);
}

void PhuArpAudioProcessorEditor::addLogMessage(const juce::String& message)
{
    // Get current time
    auto time = juce::Time::getCurrentTime();
    auto timeString = time.formatted("%H:%M:%S");
    
    // Add timestamped message
    auto logLine = "[" + timeString + "] " + message + "\n";
    logTextEditor.moveCaretToEnd();
    logTextEditor.insertTextAtCaret(logLine);
    
    // Auto-scroll to bottom
    logTextEditor.moveCaretToEnd();
}