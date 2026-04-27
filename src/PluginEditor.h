#pragma once

#include "audio/FFTProcessor.h"
#include "BandControlStrip.h"
#include "BandWaveformDisplay.h"
#include "CrossoverFrequencyBar.h"
#include "PresetStrip.h"
#include <juce_audio_processors/juce_audio_processors.h>
#ifndef NDEBUG
#include "debug/DebugLogSink.h"
#include "debug/DebugLogEventQueue.h"
#endif

using FFTProcessor = phu::audio::FFTProcessor<float>;

class PhuSplitterAudioProcessor;

class PhuSplitterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        public juce::Timer
#ifndef NDEBUG
                                        , public DebugLogSink
#endif
{
  public:
    PhuSplitterAudioProcessorEditor(PhuSplitterAudioProcessor&);
    ~PhuSplitterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

#ifndef NDEBUG // Debug builds only
    // DebugLogSink interface
    void onLogMessage(const juce::String& message) override;

    // Add log message to debug window
    void addLogMessage(const juce::String& message);
#endif

  private:
    PhuSplitterAudioProcessor& audioProcessor;

    // Preset strip (Design 3: inline navigation)
    PresetStrip presetStrip;

    // Band control strip (solo/mute buttons)
    BandControlStrip bandControlStrip;

    // Crossover frequency bar
    CrossoverFrequencyBar crossoverBar;
    juce::Label crossoverLabel;

#ifndef NDEBUG // Debug builds only
    // Debug log text area (only in debug builds)
    juce::TextEditor logTextEditor;
    juce::Label logLabel;
#endif

    // FFT processors for spectrum analysis (UI thread only)
    // Order 12 = 4096 samples = ~85ms at 48kHz (responsive updates)
    FFTProcessor inputFFT{12};
    FFTProcessor outputSumFFT{12};

    // Spectrum display controls
    juce::GroupComponent fftGroup;
    juce::Slider fftSizeSlider;
    juce::Label fftSizeLabel;
    juce::Slider attackSlider;
    juce::Label attackLabel;
    juce::Slider decaySlider;
    juce::Label decayLabel;
    juce::Slider freqSmoothSlider;
    juce::Label freqSmoothLabel;

    // FFT enable toggles
    juce::GroupComponent localFFTGroup;
    juce::ToggleButton inputFFTToggle;
    juce::ToggleButton outputFFTToggle;
    
    // Remote spectrum controls
    juce::GroupComponent remoteFFTGroup;
    juce::ToggleButton remoteFFTToggle;
    
    // Spectrum broadcast controls (broadcaster lives in processor)
    juce::ToggleButton broadcastToggle;

    // Rolling band waveform display (pre-gain + post-gain overlay)
    BandWaveformDisplay bandWaveformDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhuSplitterAudioProcessorEditor)
};