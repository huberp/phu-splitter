#pragma once

#include "AudioSampleFifo.h"
#include <juce_dsp/juce_dsp.h>

namespace phu {
namespace audio {

/**
 * FFT processor for spectrum analysis, designed to run on the UI thread.
 *
 * Reads samples from an AudioSampleFifo<2> (stereo), applies windowing,
 * computes FFT, and produces a smoothed magnitude spectrum for visualization.
 *
 * Uses a sliding window approach: a local mono ring buffer holds the last
 * fftSize samples. Each process() call drains only the NEW samples from the
 * FIFO and shifts them in, then recomputes the FFT. This decouples the
 * display update rate from the FFT size — the spectrum updates at the full
 * timer rate (e.g. 60 Hz) regardless of whether the FFT is 1024 or 32768.
 *
 * Features:
 * - Configurable FFT size (via setFFTOrder)
 * - Sliding window: update rate independent of FFT size
 * - Temporal smoothing (attack/decay) to prevent jumpy visualization
 * - Frequency smoothing (averaging adjacent bins) for smoother curves
 * - SIMD-aligned buffers for optimal performance
 */
class FFTProcessor {
  public:
    /**
     * Create an FFT processor with the specified FFT order.
     * @param fftOrder  Log2 of FFT size (e.g., 14 = 16384 samples, 13 = 8192, etc.)
     */
    explicit FFTProcessor(int fftOrder = 14)
        : attackCoefficient(0.0f), decayCoefficient(0.0f), frequencySmoothingStrength(0.3f) {
        setFFTOrder(fftOrder);
    }

    /**
     * Set FFT order (log2 of FFT size).
     * This rebuilds the internal buffers and recomputes the window.
     * @param order  Log2 of FFT size (10-15 recommended: 1024-32768 samples)
     */
    void setFFTOrder(int order) {
        order = juce::jlimit(10, 15, order); // Clamp to reasonable range
        if (order == currentFFTOrder)
            return;

        currentFFTOrder = order;
        fftSize = 1 << order;

        // Recreate FFT engine
        fft = std::make_unique<juce::dsp::FFT>(order);

        // Resize buffers
        fftData.setSize(2, fftSize * 2, false, true, true); // 2 channels, complex data
        window.setSize(1, fftSize, false, true, true);
        magnitudeSpectrum.setSize(1, fftSize / 2, false, true, true);
        smoothedMagnitudeSpectrum.setSize(1, fftSize / 2, false, true, true);

        // Sliding window mono ring buffer (stores mixed-to-mono samples)
        monoRingBuffer.setSize(1, fftSize, false, true, true);
        monoRingBuffer.clear();
        monoWritePos = 0;
        monoBufferFilled = false;

        // Pre-compute Hann window
        // w(n) = 0.5 * (1 - cos(2*pi*n / (N-1)))
        auto* windowData = window.getWritePointer(0);
        for (int i = 0; i < fftSize; ++i) {
            windowData[i] =
                0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi *
                                        static_cast<float>(i) / static_cast<float>(fftSize - 1)));
        }

        // Zero-initialize magnitude spectra
        magnitudeSpectrum.clear();
        smoothedMagnitudeSpectrum.clear();
    }

    /**
     * Set temporal smoothing parameters.
     * Attack: how quickly the spectrum responds to increasing levels (0.0 = instant, 1.0 = very slow)
     * Decay: how quickly the spectrum responds to decreasing levels (0.0 = instant, 1.0 = very slow)
     * @param attack  Coefficient for rising magnitudes (0.0 - 1.0, typical: 0.5 - 0.9)
     * @param decay   Coefficient for falling magnitudes (0.0 - 1.0, typical: 0.9 - 0.99)
     */
    void setTemporalSmoothing(float attack, float decay) {
        attackCoefficient = juce::jlimit(0.0f, 1.0f, attack);
        decayCoefficient = juce::jlimit(0.0f, 1.0f, decay);
    }

    /**
     * Set frequency smoothing strength (averaging adjacent bins).
     * @param strength  Smoothing strength (0.0 = no smoothing, 1.0 = maximum smoothing, typical: 0.2 - 0.5)
     */
    void setFrequencySmoothing(float strength) {
        frequencySmoothingStrength = juce::jlimit(0.0f, 1.0f, strength);
    }

    /**
     * Process samples from the FIFO and compute smoothed FFT magnitude spectrum.
     *
     * Sliding window approach: drains only the NEW samples from the FIFO,
     * shifts them into a local mono ring buffer, then recomputes the FFT
     * over the full window. This means the update rate equals the timer rate
     * (e.g. 60 Hz) regardless of FFT size.
     *
     * Call this from the UI thread (e.g., in a Timer).
     *
     * @param fifo  The audio sample FIFO to read from (stereo).
     * @return      True if processing succeeded (ring buffer fully populated), false otherwise.
     */
    bool process(AudioSampleFifo<2>& fifo) {
        const int available = fifo.getNumAvailable();
        if (available <= 0)
            return monoBufferFilled; // No new data; reuse last spectrum if we have one

        // Read ALL available new samples from the FIFO (consume them)
        // Use a reasonably-sized temp buffer to avoid huge allocations
        const int toRead = juce::jmin(available, fftSize); // cap to fftSize (enough for full window)
        juce::AudioBuffer<float> tempBuffer(2, toRead);
        float* channelPointers[2] = {tempBuffer.getWritePointer(0), tempBuffer.getWritePointer(1)};

        const int samplesRead = fifo.pull(channelPointers, toRead);
        if (samplesRead <= 0)
            return monoBufferFilled;

        // Mix stereo to mono and shift into the sliding window ring buffer
        auto* ringData = monoRingBuffer.getWritePointer(0);
        const auto* left = tempBuffer.getReadPointer(0);
        const auto* right = tempBuffer.getReadPointer(1);

        for (int i = 0; i < samplesRead; ++i) {
            ringData[monoWritePos] = (left[i] + right[i]) * 0.5f;
            monoWritePos = (monoWritePos + 1) % fftSize;
        }

        // Track whether we've accumulated at least one full window
        if (!monoBufferFilled) {
            monoSamplesAccumulated += samplesRead;
            if (monoSamplesAccumulated >= fftSize)
                monoBufferFilled = true;
            else
                return false; // Not enough data yet for the first FFT
        }

        // Copy ring buffer into FFT input in correct order (oldest→newest)
        // and apply Hann window in the same pass
        auto* fftInput = fftData.getWritePointer(0);
        const auto* windowData = window.getReadPointer(0);

        for (int i = 0; i < fftSize; ++i) {
            const int ringIdx = (monoWritePos + i) % fftSize; // oldest sample first
            fftInput[i] = ringData[ringIdx] * windowData[i];
        }

        // Zero the imaginary part (interleaved real/imag format required by JUCE FFT)
        for (int i = fftSize; i < fftSize * 2; ++i) {
            fftInput[i] = 0.0f;
        }

        // Perform forward FFT (in-place, interleaved complex format)
        fft->performFrequencyOnlyForwardTransform(fftInput);

        // Compute magnitude spectrum (first half of FFT, fftSize/2 bins)
        auto* magnitudes = magnitudeSpectrum.getWritePointer(0);
        auto* smoothed = smoothedMagnitudeSpectrum.getWritePointer(0);
        const int numBins = fftSize / 2;

        for (int i = 0; i < numBins; ++i) {
            // Proper FFT amplitude scaling:
            // - Divide by fftSize/2 for amplitude normalization
            // - Multiply by 2 to compensate for Hann window coherent gain (0.5)
            // - Net result: * 4 / fftSize
            float newMagnitude = fftInput[i] * 4.0f / static_cast<float>(fftSize);

            // Apply temporal smoothing (attack/decay)
            if (newMagnitude > smoothed[i]) {
                // Rising: use attack coefficient
                smoothed[i] = smoothed[i] * attackCoefficient + newMagnitude * (1.0f - attackCoefficient);
            } else {
                // Falling: use decay coefficient
                smoothed[i] = smoothed[i] * decayCoefficient + newMagnitude * (1.0f - decayCoefficient);
            }

            magnitudes[i] = smoothed[i];
        }

        // Apply frequency smoothing (3-point weighted average)
        if (frequencySmoothingStrength > 0.0f) {
            for (int i = 1; i < numBins - 1; ++i) {
                float leftBin = magnitudes[i - 1];
                float centerBin = magnitudes[i];
                float rightBin = magnitudes[i + 1];

                // Weighted average: center gets more weight
                float smoothed_val = (leftBin + centerBin * 2.0f + rightBin) * 0.25f;

                // Blend with original based on strength
                magnitudes[i] = centerBin * (1.0f - frequencySmoothingStrength) +
                                smoothed_val * frequencySmoothingStrength;
            }
        }

        return true;
    }

    /**
     * Get read-only access to the magnitude spectrum.
     * @return  Pointer to magnitude array (fftSize/2 bins).
     */
    const float* getMagnitudeSpectrum() const {
        return magnitudeSpectrum.getReadPointer(0);
    }

    /**
     * Get the number of magnitude bins (fftSize / 2).
     */
    int getNumBins() const {
        return fftSize / 2;
    }

    /**
     * Get the FFT size (number of time-domain samples).
     */
    int getFFTSize() const {
        return fftSize;
    }

    /**
     * Get the current FFT order (log2 of FFT size).
     */
    int getFFTOrder() const {
        return currentFFTOrder;
    }

    /**
     * Get the frequency in Hz corresponding to a given bin index.
     * @param bin         Bin index (0 to fftSize/2 - 1).
     * @param sampleRate  The sample rate in Hz.
     * @return            Frequency in Hz.
     */
    float getBinFrequency(int bin, float sampleRate) const {
        return (static_cast<float>(bin) * sampleRate) / static_cast<float>(fftSize);
    }

  private:
    std::unique_ptr<juce::dsp::FFT> fft;
    int currentFFTOrder = 0;
    int fftSize = 0;

    // Smoothing parameters
    float attackCoefficient;  // For rising magnitudes
    float decayCoefficient;   // For falling magnitudes
    float frequencySmoothingStrength;

    // SIMD-aligned buffers (juce::AudioBuffer uses aligned allocation internally)
    // Channel 0: time-domain input + FFT workspace (size = fftSize * 2 for complex)
    // Channel 1: unused (reserved for future stereo FFT if needed)
    juce::AudioBuffer<float> fftData;

    // Pre-computed Hann window
    juce::AudioBuffer<float> window;

    // Magnitude spectrum output (fftSize / 2 bins)
    juce::AudioBuffer<float> magnitudeSpectrum;

    // Smoothed magnitude spectrum (temporal smoothing applied)
    juce::AudioBuffer<float> smoothedMagnitudeSpectrum;

    // Sliding window: local mono ring buffer holding the last fftSize samples
    juce::AudioBuffer<float> monoRingBuffer;
    int monoWritePos = 0;
    bool monoBufferFilled = false;
    int monoSamplesAccumulated = 0;
};

} // namespace audio
} // namespace phu
