#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "EditorLogger.h"

PhuSplitterAudioProcessorEditor::PhuSplitterAudioProcessorEditor(PhuSplitterAudioProcessor& p) 
    : AudioProcessorEditor(&p), audioProcessor(p), crossoverBar(p)
{
    // Set up crossover bar label
    crossoverLabel.setText("Crossover Frequencies", juce::dontSendNotification);
    crossoverLabel.setJustificationType(juce::Justification::centredLeft);
    crossoverLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(crossoverLabel);
    
    // Add crossover frequency bar
    addAndMakeVisible(crossoverBar);
    
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
    
    // Set editor size (wider for crossover bar, taller to fit both sections)
    setSize(750, 500);
    
    // Add initial welcome message
    addLogMessage("PhuSplitter Debug Log initialized");
}

PhuSplitterAudioProcessorEditor::~PhuSplitterAudioProcessorEditor() 
{
    // Unregister from logger
    if (auto* logger = audioProcessor.getEditorLogger())
    {
        logger->clearEditor();
    }
}

void PhuSplitterAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PhuSplitterAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    // Crossover section at top
    crossoverLabel.setBounds(area.removeFromTop(25));
    area.removeFromTop(3);
    crossoverBar.setBounds(area.removeFromTop(100));
    area.removeFromTop(10);
    
    // Debug log section below
    logLabel.setBounds(area.removeFromTop(25));
    area.removeFromTop(5);
    logTextEditor.setBounds(area);
}

void PhuSplitterAudioProcessorEditor::addLogMessage(const juce::String& message)
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