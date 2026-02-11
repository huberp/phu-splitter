#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>

class PhuSplitterAudioProcessor;

/**
 * BandControlStrip: A vertical strip with solo and mute buttons for each band.
 * 
 * Layout: 2 columns (Solo | Mute) × 7 rows (one per band)
 * - Solo buttons: "S" icon, yellow when active
 * - Mute buttons: "M" icon, red when active
 * - Buttons update from parameter automation via timer callback
 */
class BandControlStrip : public juce::Component, public juce::Timer {
  public:
    static constexpr size_t NUM_BANDS = 7;

    // Dimensions
    static constexpr int SOLO_BUTTON_WIDTH = 28;
    static constexpr int MUTE_BUTTON_WIDTH = 28;
    static constexpr int COLUMN_GAP = 4;
    static constexpr int TOTAL_WIDTH = SOLO_BUTTON_WIDTH + COLUMN_GAP + MUTE_BUTTON_WIDTH;

    // Band colours (matching CrossoverFrequencyBar)
    static constexpr std::array<juce::uint32, NUM_BANDS> BAND_COLOURS = {
        0xFF8B0000u, // Dark red      - Sub bass
        0xFFCC5500u, // Burnt orange   - Bass
        0xFFCCCC00u, // Yellow         - Low-mid
        0xFF228B22u, // Forest green   - Mid
        0xFF006699u, // Steel blue     - Upper-mid
        0xFF4B0082u, // Indigo         - Presence
        0xFF800080u  // Purple         - Brilliance
    };

    static constexpr std::array<const char*, NUM_BANDS> BAND_NAMES = {
        "SUB", "BASS", "LO-MID", "MID", "HI-MID", "PRES", "BRILL"};

    BandControlStrip(PhuSplitterAudioProcessor& processor);
    ~BandControlStrip() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Timer callback to poll parameter values (for automation)
    void timerCallback() override;

  private:
    // Button click handlers
    void onSoloButtonClicked(size_t bandIndex);
    void onMuteButtonClicked(size_t bandIndex);

    // Sync button states from parameters
    void updateButtonStates();

    PhuSplitterAudioProcessor& processorRef;

    // Solo and mute buttons for each band
    std::array<std::unique_ptr<juce::TextButton>, NUM_BANDS> soloButtons;
    std::array<std::unique_ptr<juce::TextButton>, NUM_BANDS> muteButtons;

    // Previous state to avoid unnecessary updates
    std::array<bool, NUM_BANDS> previousSoloStates{};
    std::array<bool, NUM_BANDS> previousMuteStates{};
    bool previousAnySolo = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandControlStrip)
};
