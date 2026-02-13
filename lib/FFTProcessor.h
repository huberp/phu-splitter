#pragma once

#include "AudioSampleFifo.h"
#include <juce_dsp/juce_dsp.h>

/**
 * FFT processor for spectrum analysis, designed to run on the UI thread.
 *
 * Reads samples from an AudioSampleFifo<2> (stereo), applies windowing,
 * computes FFT, and produces a smoothed magnitude spectrum for visualization.
 *
 * Features:
 * - Configurable FFT size (via setFFTOrder)
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
     * Reads the most recent fftSize samples, applies windowing, computes FFT,
     * applies temporal and frequency smoothing, and stores result.
     * Call this from the UI thread (e.g., in a Timer).
     *
     * @param fifo  The audio sample FIFO to read from (stereo).
     * @return      True if processing succeeded (enough samples available), false otherwise.
     */
    bool process(AudioSampleFifo<2>& fifo) {
        // Check if enough samples are available for a full FFT window
        if (fifo.getNumAvailable() < fftSize)
            return false;

        // Allocate temp buffers for reading stereo samples from FIFO
        // AudioBuffer is SIMD-aligned by default for optimal memcpy/processing
        juce::AudioBuffer<float> tempBuffer(2, fftSize);
        float* channelPointers[2] = {tempBuffer.getWritePointer(0), tempBuffer.getWritePointer(1)};

        // Pull most recent fftSize samples from FIFO
        const int samplesRead = fifo.pull(channelPointers, fftSize);
        if (samplesRead < fftSize)
            return false;

        // Mix stereo to mono: (L + R) / 2
        // Use channel 0 of fftData for time-domain signal
        auto* fftInput = fftData.getWritePointer(0);
        const auto* left = tempBuffer.getReadPointer(0);
        const auto* right = tempBuffer.getReadPointer(1);
        const auto* windowData = window.getReadPointer(0);

        for (int i = 0; i < fftSize; ++i) {
            // Mix to mono and apply window in one pass
            fftInput[i] = ((left[i] + right[i]) * 0.5f) * windowData[i];
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
};
