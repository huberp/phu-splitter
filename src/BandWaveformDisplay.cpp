#include "BandWaveformDisplay.h"

BandWaveformDisplay::BandWaveformDisplay() {
    setOpaque(true);
}

void BandWaveformDisplay::setSampleRate(double sr) {
    if (sr > 0.0)
        sampleRate = sr;
}

void BandWaveformDisplay::setDisplayDuration(float durationMs) {
    displayDurationMs = juce::jlimit(10.0f, 1000.0f, durationMs);
}

// ─────────────────────────────────────────────────────────────────────────
// Ring buffer helpers
// ─────────────────────────────────────────────────────────────────────────

void BandWaveformDisplay::appendToRing(float* ringData, int& writePos,
                                        int& samplesWritten,
                                        const float* monoSamples, int count) {
    for (int i = 0; i < count; ++i) {
        ringData[writePos] = monoSamples[i];
        writePos = (writePos + 1) % kRingSize;
    }
    samplesWritten = juce::jmin(samplesWritten + count, kRingSize);
}

void BandWaveformDisplay::readFromRing(const float* ringData, int writePos,
                                        int samplesWritten, float* dest,
                                        int count) {
    // Read the most recent 'count' samples (oldest first → newest last)
    const int available = juce::jmin(samplesWritten, count);
    // Start reading from (writePos - available), wrapped
    int readPos = (writePos - available + kRingSize) % kRingSize;
    for (int i = 0; i < available; ++i) {
        dest[i] = ringData[readPos];
        readPos = (readPos + 1) % kRingSize;
    }
    // Zero-fill if we don't have enough history yet
    for (int i = available; i < count; ++i)
        dest[i] = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────
// FIFO → ring buffer transfer (called from UI timerCallback)
// ─────────────────────────────────────────────────────────────────────────

void BandWaveformDisplay::updateFromFifos(
    std::array<AudioSampleFifo<2>, NUM_BANDS>& preGainFifos,
    std::array<AudioSampleFifo<2>, NUM_BANDS>& postGainFifos) {

    for (int band = 0; band < NUM_BANDS; ++band) {
        auto& ring = rings[static_cast<size_t>(band)];

        // Pull ALL available pre-gain samples (stereo → mono → ring)
        {
            const int avail = preGainFifos[static_cast<size_t>(band)].getNumAvailable();
            const int toPull = juce::jmin(avail, kMaxPullSamples);
            if (toPull > 0) {
                float* ch[2] = {tempL.data(), tempR.data()};
                int got = preGainFifos[static_cast<size_t>(band)].pull(ch, toPull);
                // Convert stereo to mono in-place in tempL
                for (int i = 0; i < got; ++i)
                    tempL[static_cast<size_t>(i)] =
                        (tempL[static_cast<size_t>(i)] + tempR[static_cast<size_t>(i)]) * 0.5f;
                appendToRing(ring.preGain.data(), ring.writePos, ring.samplesWritten,
                             tempL.data(), got);
            }
        }

        // Pull ALL available post-gain samples (stereo → mono → post-gain ring)
        // Post-gain data arrives at the same rate as pre-gain from processBlock,
        // so we write at the same positions (writePos was just advanced by pre-gain).
        {
            const int avail = postGainFifos[static_cast<size_t>(band)].getNumAvailable();
            const int toPull = juce::jmin(avail, kMaxPullSamples);
            if (toPull > 0) {
                float* ch[2] = {tempL.data(), tempR.data()};
                int got = postGainFifos[static_cast<size_t>(band)].pull(ch, toPull);
                for (int i = 0; i < got; ++i)
                    tempL[static_cast<size_t>(i)] =
                        (tempL[static_cast<size_t>(i)] + tempR[static_cast<size_t>(i)]) * 0.5f;
                // Write into postGain ring at the same positions that preGain
                // just wrote to (they arrive in lockstep from processBlock).
                int postWritePos = (ring.writePos - got + kRingSize) % kRingSize;
                for (int i = 0; i < got; ++i) {
                    ring.postGain[static_cast<size_t>(postWritePos)] = tempL[static_cast<size_t>(i)];
                    postWritePos = (postWritePos + 1) % kRingSize;
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────
// Rendering
// ─────────────────────────────────────────────────────────────────────────

void BandWaveformDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xFF1A1A1Au));

    // How many samples to display
    const int displaySamples = juce::jmin(
        static_cast<int>(sampleRate * static_cast<double>(displayDurationMs) / 1000.0),
        kRingSize);

    const int gap = 2;
    const int columnWidth = (bounds.getWidth() - (NUM_BANDS - 1) * gap) / NUM_BANDS;

    for (int band = 0; band < NUM_BANDS; ++band) {
        auto col = bounds.removeFromLeft(columnWidth);
        if (band < NUM_BANDS - 1)
            bounds.removeFromLeft(gap);

        auto bandColour = juce::Colour(BAND_COLOURS[static_cast<size_t>(band)]);

        // Subtle band background
        g.setColour(bandColour.withAlpha(0.08f));
        g.fillRect(col);

        // Band name label at top
        auto labelArea = col.removeFromTop(14);
        g.setColour(bandColour.withAlpha(0.7f));
        g.setFont(juce::Font(10.0f));
        g.drawText(BAND_NAMES[static_cast<size_t>(band)], labelArea,
                   juce::Justification::centred);

        // Centre line
        const float centreY = static_cast<float>(col.getCentreY());
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawHorizontalLine(static_cast<int>(centreY),
                             static_cast<float>(col.getX()),
                             static_cast<float>(col.getRight()));

        const auto& ring = rings[static_cast<size_t>(band)];

        // Read most recent displaySamples from pre-gain ring
        readFromRing(ring.preGain.data(), ring.writePos, ring.samplesWritten,
                     paintBufPre.data(), displaySamples);

        // Read most recent displaySamples from post-gain ring
        readFromRing(ring.postGain.data(), ring.writePos, ring.samplesWritten,
                     paintBufPost.data(), displaySamples);

        // Draw pre-gain waveform (transparent overlay)
        drawWaveform(g, col, paintBufPre.data(), displaySamples, bandColour, 0.35f);

        // Draw post-gain waveform (full colour)
        drawWaveform(g, col, paintBufPost.data(), displaySamples, bandColour, 1.0f);
    }
}

void BandWaveformDisplay::drawWaveform(juce::Graphics& g,
                                        const juce::Rectangle<int>& area,
                                        const float* samples, int numSamples,
                                        juce::Colour colour, float alpha) const {
    g.setColour(colour.withAlpha(alpha));

    const int w = area.getWidth();
    if (w <= 0 || numSamples <= 0)
        return;

    const float centreY = static_cast<float>(area.getCentreY());
    const float halfHeight = static_cast<float>(area.getHeight()) * 0.45f;
    const float samplesPerPixel =
        static_cast<float>(numSamples) / static_cast<float>(w);

    for (int px = 0; px < w; ++px) {
        const int startSamp = static_cast<int>(static_cast<float>(px) * samplesPerPixel);
        int endSamp = static_cast<int>(static_cast<float>(px + 1) * samplesPerPixel);
        endSamp = juce::jmin(endSamp, numSamples);

        float minVal = 0.0f;
        float maxVal = 0.0f;
        for (int s = startSamp; s < endSamp; ++s) {
            minVal = juce::jmin(minVal, samples[static_cast<size_t>(s)]);
            maxVal = juce::jmax(maxVal, samples[static_cast<size_t>(s)]);
        }

        // Clamp to [-1, 1] for display
        minVal = juce::jlimit(-1.0f, 1.0f, minVal);
        maxVal = juce::jlimit(-1.0f, 1.0f, maxVal);

        const float y1 = centreY - maxVal * halfHeight;
        const float y2 = centreY - minVal * halfHeight;

        // Ensure at least 1px line even for very quiet signals
        if (y2 - y1 < 1.0f) {
            g.drawVerticalLine(area.getX() + px,
                               centreY - 0.5f, centreY + 0.5f);
        } else {
            g.drawVerticalLine(area.getX() + px, y1, y2);
        }
    }
}
