#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class PhuSplitterAudioProcessor;

/**
 * CrossoverFrequencyBar: A horizontal bar showing 7 coloured band regions
 * with 6 draggable vertical dividers for crossover frequencies.
 * 
 * Below the bar sit 6 editable text boxes showing each crossover frequency.
 * Double-click a box to type a value; press Return to commit.
 * Dragging a divider updates the text box and vice-versa.
 * 
 * Dividers are constrained so they can never cross their neighbours.
 */
class CrossoverFrequencyBar : public juce::Component,
                              public juce::Timer
{
public:
    static constexpr size_t NUM_BANDS = 7;
    static constexpr size_t NUM_FREQS = 6;
    
    // Frequency display range (log axis)
    static constexpr float MIN_FREQ = 20.0f;
    static constexpr float MAX_FREQ = 20000.0f;
    
    // Minimum octave gap between neighbouring dividers (~1/3 octave)
    static constexpr float MIN_FREQ_RATIO = 1.26f;

    CrossoverFrequencyBar(PhuSplitterAudioProcessor& processor);
    ~CrossoverFrequencyBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Mouse interaction for divider dragging
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    // Timer callback to poll parameter values (for automation)
    void timerCallback() override;

private:
    // Band colours (visually distinct)
    static constexpr std::array<juce::uint32, NUM_BANDS> BAND_COLOURS = {
        0xFF8B0000u,  // Dark red      - Sub bass
        0xFFCC5500u,  // Burnt orange   - Bass
        0xFFCCCC00u,  // Yellow         - Low-mid
        0xFF228B22u,  // Forest green   - Mid
        0xFF006699u,  // Steel blue     - Upper-mid
        0xFF4B0082u,  // Indigo         - Presence
        0xFF800080u   // Purple         - Brilliance
    };
    
    static constexpr std::array<const char*, NUM_BANDS> BAND_NAMES = {
        "SUB", "BASS", "LO-MID", "MID", "HI-MID", "PRES", "BRILL"
    };
    
    // --- Coordinate conversion (log scale) ---
    float freqToX(float freq) const;
    float xToFreq(float x) const;
    
    // --- Frequency validation ---
    float clampFreqForIndex(size_t index, float freq) const;
    
    // --- Text box handling ---
    void createTextBoxes();
    void updateTextBoxFromFreq(size_t index);
    void onTextBoxReturnKey(size_t index);
    
    // --- Sync ---
    void pushFreqToParam(size_t index, float freq);
    void pullFreqsFromParams();
    
    // --- Layout helpers ---
    juce::Rectangle<int> getBarArea() const;
    juce::Rectangle<int> getTextBoxArea() const;
    int getDividerHitZoneHalfWidth() const { return 6; }
    int dividerXForIndex(size_t index) const;
    int hitTestDivider(int x) const;  // returns index or -1

    PhuSplitterAudioProcessor& processorRef;
    
    // Current crossover frequencies (in Hz)
    std::array<float, NUM_FREQS> freqs;
    
    // Text boxes for frequency readout / input
    std::array<std::unique_ptr<juce::Label>, NUM_FREQS> freqLabels;
    
    // Interaction state
    int dragIndex = -1;
    int hoverIndex = -1;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossoverFrequencyBar)
};
