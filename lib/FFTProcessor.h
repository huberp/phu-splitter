#pragma once

#include "AudioSampleFifo.h"
#include <juce_dsp/juce_dsp.h>

/**
 * FFT processor for spectrum analysis, designed to run on the UI thread.
 *
 * Reads samples from an AudioSampleFifo<2> (stereo), applies windowing,
 * computes FFT, and produces a magnitude spectrum for visualization.
 *
 * Uses SIMD-aligned buffers (via juce::AudioBuffer) for optimal performance.
 */
class FFTProcessor {
  public:
    /**
     * Create an FFT processor with the specified FFT order.
     * @param fftOrder  Log2 of FFT size (e.g., 14 = 16384 samples, 13 = 8192, etc.)
     */
    explicit FFTProcessor(int fftOrder = 14)
        : fft(fftOrder), fftSize(1 << fftOrder), fftData(2, fftSize * 2) // 2 channels, complex data
          ,
          window(1, fftSize), magnitudeSpectrum(1, fftSize / 2) {
        // Pre-compute Hann window
        // w(n) = 0.5 * (1 - cos(2*pi*n / (N-1)))
        auto* windowData = window.getWritePointer(0);
        for (int i = 0; i < fftSize; ++i) {
            windowData[i] =
                0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(i) /
                                        static_cast<float>(fftSize - 1)));
        }

        // Zero-initialize magnitude spectrum
        magnitudeSpectrum.clear();
    }

    /**
     * Process samples from the FIFO and compute FFT magnitude spectrum.
     * Reads the most recent fftSize samples, applies windowing, computes FFT,
     * and stores magnitude spectrum. Call this from the UI thread (e.g., in a Timer).
     *
     * @param fifo  The audio sample FIFO to read from (stereo).
     * @return      True if processing succeeded (enough samples available), false otherwise.
     */
    bool process(AudioSampleFifo<2>& fifo) {
        // Check if enough samples are available
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
        fft.performFrequencyOnlyForwardTransform(fftInput);

        // Compute magnitude spectrum (first half of FFT, fftSize/2 bins)
        // FFT output is [DC, f1, f2, ... fN/2-1, Nyquist]
        auto* magnitudes = magnitudeSpectrum.getWritePointer(0);
        const int numBins = fftSize / 2;

        for (int i = 0; i < numBins; ++i) {
            // The performFrequencyOnlyForwardTransform stores magnitudes directly
            // Scale by 1/fftSize for proper amplitude
            magnitudes[i] = fftInput[i] / static_cast<float>(fftSize);
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
     * Get the frequency in Hz corresponding to a given bin index.
     * @param bin         Bin index (0 to fftSize/2 - 1).
     * @param sampleRate  The sample rate in Hz.
     * @return            Frequency in Hz.
     */
    float getBinFrequency(int bin, float sampleRate) const {
        return (static_cast<float>(bin) * sampleRate) / static_cast<float>(fftSize);
    }

  private:
    juce::dsp::FFT fft;
    int fftSize;

    // SIMD-aligned buffers (juce::AudioBuffer uses aligned allocation internally)
    // Channel 0: time-domain input + FFT workspace (size = fftSize * 2 for complex)
    // Channel 1: unused (reserved for future stereo FFT if needed)
    juce::AudioBuffer<float> fftData;

    // Pre-computed Hann window
    juce::AudioBuffer<float> window;

    // Magnitude spectrum output (fftSize / 2 bins)
    juce::AudioBuffer<float> magnitudeSpectrum;
};
