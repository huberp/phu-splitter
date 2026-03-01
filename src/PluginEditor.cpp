#include "PluginEditor.h"
#ifndef NDEBUG
#include "../lib/debug/EditorLogger.h"
#endif
#include "PluginProcessor.h"

PhuSplitterAudioProcessorEditor::PhuSplitterAudioProcessorEditor(PhuSplitterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), presetStrip(p), bandControlStrip(p),
      crossoverBar(p, &inputFFT, &outputSumFFT) {
    // Add preset strip
    addAndMakeVisible(presetStrip);

    // Add band control strip
    addAndMakeVisible(bandControlStrip);

    // Set up crossover bar label
    crossoverLabel.setText("Crossover Frequencies", juce::dontSendNotification);
    crossoverLabel.setJustificationType(juce::Justification::centredLeft);
    crossoverLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    addAndMakeVisible(crossoverLabel);

    // Add crossover frequency bar
    addAndMakeVisible(crossoverBar);

    // Set up spectrum control sliders
    fftSizeLabel.setText("FFT Size", juce::dontSendNotification);
    fftSizeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(fftSizeLabel);

    fftSizeSlider.setRange(10, 15, 1); // 1024 to 32768
    fftSizeSlider.setValue(12, juce::dontSendNotification);
    fftSizeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fftSizeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    fftSizeSlider.onValueChange = [this]() {
        int order = static_cast<int>(fftSizeSlider.getValue());
        inputFFT.setFFTOrder(order);
        outputSumFFT.setFFTOrder(order);
    };
    addAndMakeVisible(fftSizeSlider);

    attackLabel.setText("Attack", juce::dontSendNotification);
    attackLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(attackLabel);

    attackSlider.setRange(0.0, 1.0, 0.01);
    attackSlider.setValue(0.0, juce::dontSendNotification);
    attackSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    attackSlider.onValueChange = [this]() {
        float attack = static_cast<float>(attackSlider.getValue());
        float decay = static_cast<float>(decaySlider.getValue());
        inputFFT.setTemporalSmoothing(attack, decay);
        outputSumFFT.setTemporalSmoothing(attack, decay);
    };
    addAndMakeVisible(attackSlider);

    decayLabel.setText("Decay", juce::dontSendNotification);
    decayLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(decayLabel);

    decaySlider.setRange(0.0, 1.0, 0.01);
    decaySlider.setValue(0.0, juce::dontSendNotification);
    decaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    decaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    decaySlider.onValueChange = [this]() {
        float attack = static_cast<float>(attackSlider.getValue());
        float decay = static_cast<float>(decaySlider.getValue());
        inputFFT.setTemporalSmoothing(attack, decay);
        outputSumFFT.setTemporalSmoothing(attack, decay);
    };
    addAndMakeVisible(decaySlider);

    freqSmoothLabel.setText("Freq Smooth", juce::dontSendNotification);
    freqSmoothLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(freqSmoothLabel);

    freqSmoothSlider.setRange(0.0, 1.0, 0.01);
    freqSmoothSlider.setValue(0.3, juce::dontSendNotification);
    freqSmoothSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    freqSmoothSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    freqSmoothSlider.onValueChange = [this]() {
        float strength = static_cast<float>(freqSmoothSlider.getValue());
        inputFFT.setFrequencySmoothing(strength);
        outputSumFFT.setFrequencySmoothing(strength);
    };
    addAndMakeVisible(freqSmoothSlider);

    // FFT controls group
    fftGroup.setText("FFT");
    fftGroup.setTextLabelPosition(juce::Justification::centredLeft);
    addAndMakeVisible(fftGroup);

    // Local FFT group
    localFFTGroup.setText("Local Spectrum");
    localFFTGroup.setTextLabelPosition(juce::Justification::centredLeft);
    addAndMakeVisible(localFFTGroup);

    // FFT enable toggles
    inputFFTToggle.setButtonText("Input FFT");
    inputFFTToggle.setToggleState(false, juce::dontSendNotification);
    inputFFTToggle.onClick = [this]() {
        crossoverBar.setInputFFTEnabled(inputFFTToggle.getToggleState());
        crossoverBar.repaint();
    };
    addAndMakeVisible(inputFFTToggle);

    outputFFTToggle.setButtonText("Output FFT");
    outputFFTToggle.setToggleState(true, juce::dontSendNotification);
    outputFFTToggle.onClick = [this]() {
        crossoverBar.setOutputFFTEnabled(outputFFTToggle.getToggleState());
        crossoverBar.repaint();
    };
    addAndMakeVisible(outputFFTToggle);

    // Remote FFT group
    remoteFFTGroup.setText("Remote Spectrum");
    remoteFFTGroup.setTextLabelPosition(juce::Justification::centredLeft);
    addAndMakeVisible(remoteFFTGroup);

    remoteFFTToggle.setButtonText("Show Remote FFT");
    remoteFFTToggle.setToggleState(true, juce::dontSendNotification);
    remoteFFTToggle.onClick = [this]() {
        bool enabled = remoteFFTToggle.getToggleState();
        audioProcessor.setReceiveEnabled(enabled);
        crossoverBar.setRemoteFFTEnabled(enabled);
        crossoverBar.repaint();
    };
    addAndMakeVisible(remoteFFTToggle);

    // Broadcast controls (no separate label needed - inline with Remote FFT)

    broadcastToggle.setButtonText("Broadcast Spectrum");
    broadcastToggle.setToggleState(audioProcessor.isBroadcastEnabled(), juce::dontSendNotification);
    broadcastToggle.onClick = [this]() {
        bool enabled = broadcastToggle.getToggleState();
        audioProcessor.setBroadcastEnabled(enabled);
#ifndef NDEBUG
        if (enabled) {
            addLogMessage("Spectrum broadcasting enabled (ID: " + 
                         juce::String(audioProcessor.getSpectrumBroadcaster().getInstanceID()) + ")");
        } else {
            addLogMessage("Spectrum broadcasting disabled");
        }
#endif
    };
    addAndMakeVisible(broadcastToggle);

    // Initialize FFT processors with slider values
    inputFFT.setFFTOrder(static_cast<int>(fftSizeSlider.getValue()));
    outputSumFFT.setFFTOrder(static_cast<int>(fftSizeSlider.getValue()));
    inputFFT.setTemporalSmoothing(static_cast<float>(attackSlider.getValue()),
                                  static_cast<float>(decaySlider.getValue()));
    outputSumFFT.setTemporalSmoothing(static_cast<float>(attackSlider.getValue()),
                                      static_cast<float>(decaySlider.getValue()));
    inputFFT.setFrequencySmoothing(static_cast<float>(freqSmoothSlider.getValue()));
    outputSumFFT.setFrequencySmoothing(static_cast<float>(freqSmoothSlider.getValue()));

    // Start FFT processing timer at 60 Hz (UI thread)
    startTimerHz(60);

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
    // Layout: area(10) + preset(30+6) + label(25+3) + crossover(220+10) + 
    //         FFT group(87+8) + Local(60+8) + Remote(60+8) = 535px
    // Debug build: add ~150px for debug log
    setSize(810, 690);

    // Add initial welcome message
    addLogMessage("PhuSplitter Debug Log initialized");
#else
    // Smaller editor size for release builds (no debug log)
    // Release build: height for all controls + FFT groups + margins
    setSize(810, 540);
#endif
}

PhuSplitterAudioProcessorEditor::~PhuSplitterAudioProcessorEditor() {
    // Stop FFT timer
    stopTimer();

    // Broadcaster is owned by processor — do NOT shutdown here

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

    // ============================================================================
    // UI Layout Constants - keep consistent across all groups
    // ============================================================================
    constexpr int kRowHeight = 24;           // Height of one row of controls
    constexpr int kRowGap = 3;               // Gap between rows
    constexpr int kGroupPaddingV = 18;       // Vertical padding (title area + bottom)
    constexpr int kGroupPaddingH = 10;       // Horizontal padding
    constexpr int kToggleGap = 10;           // Gap between toggle buttons
    constexpr int kGroupSpacing = 8;         // Spacing between groups
    
    // Helper lambda to compute group height for N rows of controls
    auto computeGroupHeight = [&](int numRows) {
        int contentHeight = numRows * kRowHeight + (numRows > 1 ? (numRows - 1) * kRowGap : 0);
        return 2 * kGroupPaddingV + contentHeight;
    };
    
    // Helper lambda to layout a pair of toggle buttons consistently
    auto layoutTogglePair = [&](juce::Rectangle<int>& row, juce::ToggleButton& left, juce::ToggleButton& right) {
        int toggleWidth = (row.getWidth() - kToggleGap) / 2;
        left.setBounds(row.removeFromLeft(toggleWidth));
        row.removeFromLeft(kToggleGap);
        right.setBounds(row.removeFromLeft(toggleWidth));
    };
    // ============================================================================

    // FFT controls group (compact 2x2 grid)
    auto fftGroupArea = area.removeFromTop(computeGroupHeight(2));
    fftGroup.setBounds(fftGroupArea);
    auto controlArea = fftGroupArea.reduced(kGroupPaddingH, kGroupPaddingV);
    const int labelWidth = 70;
    const int sliderWidth = (controlArea.getWidth() - labelWidth * 2 - 20) / 2;

    // Top row: FFT Size | Attack
    auto topRow = controlArea.removeFromTop(kRowHeight);
    fftSizeLabel.setBounds(topRow.removeFromLeft(labelWidth));
    fftSizeSlider.setBounds(topRow.removeFromLeft(sliderWidth));
    topRow.removeFromLeft(10); // Gap
    attackLabel.setBounds(topRow.removeFromLeft(labelWidth));
    attackSlider.setBounds(topRow.removeFromLeft(sliderWidth));

    controlArea.removeFromTop(kRowGap);

    // Bottom row: Decay | Freq Smooth
    auto bottomRow = controlArea.removeFromTop(kRowHeight);
    decayLabel.setBounds(bottomRow.removeFromLeft(labelWidth));
    decaySlider.setBounds(bottomRow.removeFromLeft(sliderWidth));
    bottomRow.removeFromLeft(10); // Gap
    freqSmoothLabel.setBounds(bottomRow.removeFromLeft(labelWidth));
    freqSmoothSlider.setBounds(bottomRow.removeFromLeft(sliderWidth));

    area.removeFromTop(kGroupSpacing);

    // Local FFT toggles group (1 row: Output on left, Input on right)
    auto localFFTArea = area.removeFromTop(computeGroupHeight(1));
    localFFTGroup.setBounds(localFFTArea);
    auto localFFTContent = localFFTArea.reduced(kGroupPaddingH, kGroupPaddingV);
    auto toggleRow = localFFTContent.removeFromTop(kRowHeight);
    layoutTogglePair(toggleRow, outputFFTToggle, inputFFTToggle);

    area.removeFromTop(kGroupSpacing);

    // Remote Spectrum group (1 row: Show Remote FFT on left, Broadcast on right)
    auto remoteFFTArea = area.removeFromTop(computeGroupHeight(1));
    remoteFFTGroup.setBounds(remoteFFTArea);
    auto remoteFFTContent = remoteFFTArea.reduced(kGroupPaddingH, kGroupPaddingV);
    auto remoteFFTRow = remoteFFTContent.removeFromTop(kRowHeight);
    layoutTogglePair(remoteFFTRow, remoteFFTToggle, broadcastToggle);

    area.removeFromTop(kGroupSpacing);

#ifndef NDEBUG // Debug builds only
    // Debug log section below (debug builds only)
    logLabel.setBounds(area.removeFromTop(25));
    area.removeFromTop(5);
    logTextEditor.setBounds(area);
#endif
}

void PhuSplitterAudioProcessorEditor::timerCallback() {
    // Process FFT on UI thread at 60 Hz (only if enabled)
    // Read from audio FIFOs and compute magnitude spectra
    if (inputFFTToggle.getToggleState())
        inputFFT.process(audioProcessor.getInputFifo());
    if (outputFFTToggle.getToggleState())
        outputSumFFT.process(audioProcessor.getOutputSumFifo());

    // Broadcast is now handled by the processor's timer — no broadcast logic here.

    // Receive remote spectrums from processor's broadcaster (independent from broadcasting)
    if (remoteFFTToggle.getToggleState()) {
        auto& broadcaster = audioProcessor.getSpectrumBroadcaster();
        auto remoteSpectrums = broadcaster.getReceivedSpectrums();
        crossoverBar.setRemoteSpectrums(remoteSpectrums);
    } else {
        crossoverBar.setRemoteSpectrums({});
    }

    // Trigger repaint of spectrum visualization
    crossoverBar.repaint();
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