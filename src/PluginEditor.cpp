#include "PluginEditor.h"
#include "EditorLogger.h"
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
    fftSizeLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
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
    attackLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
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
    decayLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
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
    freqSmoothLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
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

    remoteFFTToggle.setButtonText("Remote FFT");
    remoteFFTToggle.setToggleState(true, juce::dontSendNotification);
    remoteFFTToggle.onClick = [this]() {
        crossoverBar.setRemoteFFTEnabled(remoteFFTToggle.getToggleState());
        crossoverBar.repaint();
    };
    addAndMakeVisible(remoteFFTToggle);

    // Broadcast controls
    broadcastLabel.setText("Multicast:", juce::dontSendNotification);
    broadcastLabel.setJustificationType(juce::Justification::centredRight);
    broadcastLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    addAndMakeVisible(broadcastLabel);

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
    // Debug build: increased height for spectrum controls + broadcast controls
    setSize(810, 680);

    // Add initial welcome message
    addLogMessage("PhuSplitter Debug Log initialized");
#else
    // Smaller editor size for release builds (no debug log)
    // Release build: height for all controls + FFT toggles + broadcast controls + margins
    setSize(810, 450);
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

    // Spectrum control sliders (compact 2x2 grid)
    auto controlArea = area.removeFromTop(50);
    const int labelWidth = 70;
    const int sliderWidth = (controlArea.getWidth() - labelWidth * 2 - 20) / 2;

    // Top row: FFT Size | Attack
    auto topRow = controlArea.removeFromTop(22);
    fftSizeLabel.setBounds(topRow.removeFromLeft(labelWidth));
    fftSizeSlider.setBounds(topRow.removeFromLeft(sliderWidth));
    topRow.removeFromLeft(10); // Gap
    attackLabel.setBounds(topRow.removeFromLeft(labelWidth));
    attackSlider.setBounds(topRow.removeFromLeft(sliderWidth));

    controlArea.removeFromTop(3); // Gap between rows

    // Bottom row: Decay | Freq Smooth
    auto bottomRow = controlArea.removeFromTop(22);
    decayLabel.setBounds(bottomRow.removeFromLeft(labelWidth));
    decaySlider.setBounds(bottomRow.removeFromLeft(sliderWidth));
    bottomRow.removeFromLeft(10); // Gap
    freqSmoothLabel.setBounds(bottomRow.removeFromLeft(labelWidth));
    freqSmoothSlider.setBounds(bottomRow.removeFromLeft(sliderWidth));

    area.removeFromTop(3);

    // FFT enable toggles row
    auto toggleRow = area.removeFromTop(22);
    toggleRow.removeFromLeft(10);
    remoteFFTToggle.setBounds(toggleRow.removeFromLeft(100));

    area.removeFromTop(3);

    // Broadcast control row
    auto broadcastRow = area.removeFromTop(22);
    broadcastLabel.setBounds(broadcastRow.removeFromLeft(70));
    broadcastToggle.setBounds(broadcastRow.removeFromLeft(160));

    area.removeFromTop(8);

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

    // Receive remote spectrums from processor's broadcaster
    auto& broadcaster = audioProcessor.getSpectrumBroadcaster();
    if (broadcaster.isRunning() && remoteFFTToggle.getToggleState()) {
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