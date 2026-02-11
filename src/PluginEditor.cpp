#include "PluginEditor.h"
#include "EditorLogger.h"
#include "PluginProcessor.h"

PhuSplitterAudioProcessorEditor::PhuSplitterAudioProcessorEditor(PhuSplitterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), presetStrip(p), bandControlStrip(p),
      crossoverBar(p) {
    // Add preset strip
    addAndMakeVisible(presetStrip);

    // Add band control strip
    addAndMakeVisible(bandControlStrip);

    // Set up crossover bar label
    crossoverLabel.setText("Crossover Frequencies", juce::dontSendNotification);
    crossoverLabel.setJustificationType(juce::Justification::centredLeft);
    crossoverLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(crossoverLabel);

    // Add crossover frequency bar
    addAndMakeVisible(crossoverBar);

#ifndef NDEBUG // Debug builds only
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
    logTextEditor.setFont(
        juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    logTextEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    logTextEditor.setColour(juce::TextEditor::textColourId, juce::Colours::lightgreen);
    logTextEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::grey);
    addAndMakeVisible(logTextEditor);

    // Set editor size (wider for crossover bar, taller to fit both sections)
    // Debug build: increased from 500 to 550px height, width from 750 to 810px
    setSize(810, 550);

    // Add initial welcome message
    addLogMessage("PhuSplitter Debug Log initialized");
#else
    // Smaller editor size for release builds (no debug log)
    // Release build: increased from 200 to 250px height, width from 750 to 810px
    setSize(810, 250);
#endif
}

PhuSplitterAudioProcessorEditor::~PhuSplitterAudioProcessorEditor() {
#ifndef NDEBUG // Debug builds only
    // Unregister from logger
    if (auto* logger = audioProcessor.getEditorLogger()) {
        logger->clearEditor();
    }
#endif
}

void PhuSplitterAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PhuSplitterAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(10);

    // Preset strip at very top
    presetStrip.setBounds(area.removeFromTop(30));
    area.removeFromTop(6);

    // Crossover section
    crossoverLabel.setBounds(area.removeFromTop(25));
    area.removeFromTop(3);

    // Horizontal layout: BandControlStrip | CrossoverBar
    auto crossoverArea = area.removeFromTop(220); // Increased from 100 to 220
    
    // Band control strip on the left
    bandControlStrip.setBounds(crossoverArea.removeFromLeft(BandControlStrip::TOTAL_WIDTH));
    
    // Gap between strip and bar
    crossoverArea.removeFromLeft(6);
    
    // Crossover bar on the right
    crossoverBar.setBounds(crossoverArea);
    
    area.removeFromTop(10);

#ifndef NDEBUG // Debug builds only
    // Debug log section below (debug builds only)
    logLabel.setBounds(area.removeFromTop(25));
    area.removeFromTop(5);
    logTextEditor.setBounds(area);
#endif
}

#ifndef NDEBUG // Debug builds only
void PhuSplitterAudioProcessorEditor::addLogMessage(const juce::String& message) {
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
#endif