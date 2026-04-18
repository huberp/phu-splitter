#pragma once

#include "audio/AudioSampleFifo.h"
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

using phu::audio::AudioSampleFifo;

/**
 * BandWaveformDisplay: A rolling waveform display showing 7 band columns.
 *
 * Each column shows the pre-gain waveform (35% alpha) overlaid with the
 * post-gain waveform (full alpha) for one band. The display duration is
 * configurable (default 500ms, max 1000ms).
 *
 * Internally maintains a per-band ring buffer that accumulates data across
 * timer ticks. The AudioSampleFifos only need to buffer one tick's worth
 * of data (~800 samples at 48kHz/60Hz).
 *
 * Call updateFromFifos() from the editor's timerCallback to pull the latest
 * audio samples from the processor's per-band FIFOs.
 */
class BandWaveformDisplay : public juce::Component {
  public:
    static constexpr int NUM_BANDS = 7;

    // Ring buffer capacity: supports up to 1000ms at 96kHz
    static constexpr int kRingSize = 96000;

    // Temp pull buffer size (one timer tick at 96kHz/30Hz = 3200, with margin)
    static constexpr int kMaxPullSamples = 8192;

    BandWaveformDisplay();

    /** Set the audio sample rate (needed to convert duration to samples). */
    void setSampleRate(double sr);

    /** Set the display duration in milliseconds (default 500ms, range 10-1000ms). */
    void setDisplayDuration(float durationMs);

    /** Pull latest samples from per-band FIFOs (call from timerCallback). */
    void updateFromFifos(std::array<AudioSampleFifo<2>, NUM_BANDS>& preGainFifos,
                         std::array<AudioSampleFifo<2>, NUM_BANDS>& postGainFifos);

    void paint(juce::Graphics& g) override;

  private:
    float displayDurationMs = 500.0f;
    double sampleRate = 48000.0;

    // Per-band ring buffer (mono: average of L+R)
    struct BandRingBuffer {
        std::array<float, kRingSize> preGain{};
        std::array<float, kRingSize> postGain{};
        int writePos = 0;       // Next write position (wraps at kRingSize)
        int samplesWritten = 0; // Total samples accumulated (saturates at kRingSize)
    };
    std::array<BandRingBuffer, NUM_BANDS> rings{};

    // Temp stereo pull buffers (reused each frame)
    std::array<float, kMaxPullSamples> tempL{};
    std::array<float, kMaxPullSamples> tempR{};

    // Contiguous read-out buffers for paint() (avoid alloc per frame)
    std::array<float, kRingSize> paintBufPre{};
    std::array<float, kRingSize> paintBufPost{};

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

    /** Append mono samples into a ring buffer channel. */
    static void appendToRing(float* ringData, int& writePos, int& samplesWritten,
                             const float* monoSamples, int count);

    /** Copy the most recent N samples from a ring buffer into a contiguous output. */
    static void readFromRing(const float* ringData, int writePos, int samplesWritten,
                             float* dest, int count);

    /** Draw a single waveform (min/max envelope) into the given area. */
    void drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& area,
                      const float* samples, int numSamples,
                      juce::Colour colour, float alpha) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandWaveformDisplay)
};
