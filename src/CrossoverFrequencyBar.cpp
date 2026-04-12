#include "CrossoverFrequencyBar.h"
#include "../lib/audio/NoteToFreq.h"
#include "PluginProcessor.h"

using phu::audio::NoteToFreq;

// ============================================================================
// Construction / Destruction
// ============================================================================

CrossoverFrequencyBar::CrossoverFrequencyBar(PhuSplitterAudioProcessor& processor,
                                             FFTProcessor* inputFFT, FFTProcessor* outputSumFFT)
    : processorRef(processor), inputFFTProcessor(inputFFT), outputSumFFTProcessor(outputSumFFT) {
    // Pull initial frequencies from processor parameters
    auto initFreqs = processorRef.getCrossoverFrequencies();
    for (size_t i = 0; i < NUM_FREQS; ++i)
        freqs[i] = initFreqs[i];

    // Pull initial band gains from processor parameters
    auto initGains = processorRef.getBandGains();
    for (size_t i = 0; i < NUM_BANDS; ++i)
        bandGainsDB[i] = initGains[i];

    createTextBoxes();

    // Poll parameter changes at 15 Hz (for DAW automation reflection)
    startTimerHz(15);
}

CrossoverFrequencyBar::~CrossoverFrequencyBar() {
    stopTimer();
}

// ============================================================================
// Coordinate conversion  (logarithmic frequency axis)
// ============================================================================

float CrossoverFrequencyBar::freqToX(float freq) const {
    auto bar = getBarArea().toFloat();
    if (bar.getWidth() <= 0.0f)
        return bar.getX();

    float logMin = std::log(MIN_FREQ);
    float logMax = std::log(MAX_FREQ);
    float norm = (std::log(freq) - logMin) / (logMax - logMin);
    return bar.getX() + norm * bar.getWidth();
}

float CrossoverFrequencyBar::xToFreq(float x) const {
    auto bar = getBarArea().toFloat();
    if (bar.getWidth() <= 0.0f)
        return MIN_FREQ;

    float norm = (x - bar.getX()) / bar.getWidth();
    norm = juce::jlimit(0.0f, 1.0f, norm);
    float logMin = std::log(MIN_FREQ);
    float logMax = std::log(MAX_FREQ);
    return std::exp(logMin + norm * (logMax - logMin));
}

// ============================================================================
// Gain coordinate conversion (linear scale)
// ============================================================================

float CrossoverFrequencyBar::gainToY(float gainDB) const {
    auto bar = getBarArea().toFloat();
    if (bar.getHeight() <= 0.0f)
        return bar.getY();

    // Top = +24 dB, Bottom = -24 dB, Center = 0 dB
    float norm = (MAX_GAIN_DB - gainDB) / (MAX_GAIN_DB - MIN_GAIN_DB);
    return bar.getY() + norm * bar.getHeight();
}

float CrossoverFrequencyBar::yToGain(float y) const {
    auto bar = getBarArea().toFloat();
    if (bar.getHeight() <= 0.0f)
        return 0.0f;

    float norm = (y - bar.getY()) / bar.getHeight();
    norm = juce::jlimit(0.0f, 1.0f, norm);
    // Top = +24 dB, Bottom = -24 dB
    return MAX_GAIN_DB - norm * (MAX_GAIN_DB - MIN_GAIN_DB);
}

// ============================================================================
// Frequency validation - keep dividers in order with min gap
// ============================================================================

float CrossoverFrequencyBar::clampFreqForIndex(size_t index, float freq) const {
    float lo = MIN_FREQ;
    float hi = MAX_FREQ;

    if (index == 0) {
        // First divider: allow down to MIN_FREQ, but keep a minimum ratio to the next divider
        lo = MIN_FREQ;
        if (NUM_FREQS > 1)
            hi = freqs[1] / MIN_FREQ_RATIO;
        else
            hi = MAX_FREQ;
    } else if (index == NUM_FREQS - 1) {
        // Last divider: allow up to MAX_FREQ, but keep a minimum ratio to the previous divider
        lo = freqs[index - 1] * MIN_FREQ_RATIO;
        hi = MAX_FREQ;
    } else {
        // Middle dividers: constrained between neighboring dividers
        lo = freqs[index - 1] * MIN_FREQ_RATIO;
        hi = freqs[index + 1] / MIN_FREQ_RATIO;
    }
    return juce::jlimit(lo, hi, freq);
}

// ============================================================================
// Text boxes
// ============================================================================

static juce::String formatFreq(float hz) {
    if (hz >= 1000.0f)
        return juce::String(hz / 1000.0f, 2) + "k";
    return juce::String(static_cast<int>(std::round(hz)));
}

static float parseFreq(const juce::String& text) {
    juce::String trimmed = text.trim();
    std::string str = trimmed.toStdString();

    // Try note name first (e.g. "C#3", "E#1", "Ab4")
    if (NoteToFreq::looksLikeNoteName(str)) {
        auto freq = NoteToFreq::toFrequency(str);
        if (freq.has_value())
            return static_cast<float>(freq.value());
    }

    // Fall through to numeric parsing
    juce::String lower = trimmed.toLowerCase();

    // Handle "k" suffix for kHz
    if (lower.endsWithChar('k')) {
        float val = lower.dropLastCharacters(1).getFloatValue();
        return val * 1000.0f;
    }
    if (lower.endsWith("khz")) {
        float val = lower.dropLastCharacters(3).getFloatValue();
        return val * 1000.0f;
    }
    if (lower.endsWith("hz")) {
        return lower.dropLastCharacters(2).getFloatValue();
    }

    return lower.getFloatValue();
}

void CrossoverFrequencyBar::createTextBoxes() {
    for (size_t i = 0; i < NUM_FREQS; ++i) {
        auto label = std::make_unique<juce::Label>();
        label->setEditable(false, true, false); // not editable by single click, but by double-click
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::Font(juce::FontOptions(12.0f)));
        label->setColour(juce::Label::textColourId, juce::Colours::white);
        label->setColour(juce::Label::backgroundColourId, juce::Colour(0xFF333333));
        label->setColour(juce::Label::outlineColourId, juce::Colour(0xFF666666));
        label->setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
        label->setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xFF222222));
        label->setColour(juce::Label::outlineWhenEditingColourId, juce::Colours::cyan);

        updateTextBoxFromFreq(i); // set initial text after label is created... handled below

        const size_t idx = i;
        label->onTextChange = [this, idx]() { onTextBoxReturnKey(idx); };

        addAndMakeVisible(label.get());
        freqLabels[i] = std::move(label);

        // Now set initial text
        updateTextBoxFromFreq(i);
    }
}

void CrossoverFrequencyBar::updateTextBoxFromFreq(size_t index) {
    if (freqLabels[index])
        freqLabels[index]->setText(formatFreq(freqs[index]), juce::dontSendNotification);
}

void CrossoverFrequencyBar::onTextBoxReturnKey(size_t index) {
    if (!freqLabels[index])
        return;

    float typed = parseFreq(freqLabels[index]->getText());
    if (typed <= 0.0f) {
        // Invalid: revert to current
        updateTextBoxFromFreq(index);
        return;
    }

    // Clamp for ordering consistency
    float clamped = clampFreqForIndex(index, typed);
    freqs[index] = clamped;
    updateTextBoxFromFreq(index);
    pushFreqToParam(index, clamped);
    repaint();
}

// ============================================================================
// Push / Pull parameters
// ============================================================================

void CrossoverFrequencyBar::pushFreqToParam(size_t index, float freq) {
    processorRef.setCrossoverFrequency(index, freq);
}

void CrossoverFrequencyBar::pullFreqsFromParams() {
    auto paramFreqs = processorRef.getCrossoverFrequencies();
    bool changed = false;
    for (size_t i = 0; i < NUM_FREQS; ++i) {
        if (std::abs(paramFreqs[i] - freqs[i]) > 0.01f) {
            freqs[i] = paramFreqs[i];
            updateTextBoxFromFreq(i);
            changed = true;
        }
    }
    if (changed) {
        resized(); // reposition text boxes under new divider locations
        repaint();
    }
}

void CrossoverFrequencyBar::pushGainToParam(size_t bandIndex, float gainDB) {
    processorRef.setBandGain(bandIndex, gainDB);
}

void CrossoverFrequencyBar::pullGainsFromParams() {
    auto paramGains = processorRef.getBandGains();
    bool changed = false;
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        if (std::abs(paramGains[i] - bandGainsDB[i]) > 0.01f) {
            bandGainsDB[i] = paramGains[i];
            changed = true;
        }
    }
    if (changed)
        repaint();
}

// ============================================================================
// Snap-to-grid helper
// ============================================================================

float CrossoverFrequencyBar::applySnapToGrid(float gainDB) const {
    static constexpr float snapValues[] = {-12.0f, -6.0f, 0.0f, 6.0f, 12.0f};
    for (float snapValue : snapValues) {
        if (std::abs(gainDB - snapValue) < SNAP_THRESHOLD_DB)
            return snapValue;
    }
    return gainDB;
}

// ============================================================================
// Timer - sync with DAW automation
// ============================================================================

void CrossoverFrequencyBar::timerCallback() {
    if (dragIndex < 0 && dragGainBandIndex < 0) { // don't overwrite while user is dragging
        pullFreqsFromParams();
        pullGainsFromParams();
    }
}

// ============================================================================
// Layout helpers
// ============================================================================

juce::Rectangle<int> CrossoverFrequencyBar::getBarArea() const {
    auto area = getLocalBounds();
    // Top part is the coloured bar (increased to 220px), bottom 32px are text boxes
    return area.withTrimmedBottom(32);
}

juce::Rectangle<int> CrossoverFrequencyBar::getTextBoxArea() const {
    auto area = getLocalBounds();
    return area.removeFromBottom(28);
}

int CrossoverFrequencyBar::dividerXForIndex(size_t index) const {
    return static_cast<int>(std::round(freqToX(freqs[index])));
}

int CrossoverFrequencyBar::hitTestDivider(int x) const {
    int halfZone = getDividerHitZoneHalfWidth();
    for (size_t i = 0; i < NUM_FREQS; ++i) {
        int dx = dividerXForIndex(i);
        if (std::abs(x - dx) <= halfZone)
            return static_cast<int>(i);
    }
    return -1;
}

int CrossoverFrequencyBar::hitTestGainLine(int x, int y) const {
    auto bar = getBarArea();
    if (!bar.contains(x, y))
        return -1;

    static constexpr int GAIN_LINE_HIT_ZONE = 4;

    // Determine which band we're in horizontally
    for (size_t band = 0; band < NUM_BANDS; ++band) {
        float x0 = (band == 0) ? static_cast<float>(bar.getX()) : freqToX(freqs[band - 1]);
        float x1 =
            (band == NUM_BANDS - 1) ? static_cast<float>(bar.getRight()) : freqToX(freqs[band]);

        if (x >= x0 && x <= x1) {
            // Check if we're near the gain line for this band
            int lineY = static_cast<int>(std::round(gainToY(bandGainsDB[band])));
            if (std::abs(y - lineY) <= GAIN_LINE_HIT_ZONE)
                return static_cast<int>(band);
        }
    }
    return -1;
}

// ============================================================================
// Paint
// ============================================================================

void CrossoverFrequencyBar::paint(juce::Graphics& g) {
    auto bar = getBarArea();

    // Draw spectrum first (behind everything)
    // Output FFT: thicker white line (more prominent)
    if (outputFFTEnabled && outputSumFFTProcessor != nullptr) {
        drawSpectrum(g, bar, outputSumFFTProcessor, 2.0f, juce::Colour(0xA0FFFFFF));
    }
    // Input FFT: thinner, slightly dimmer line (for reference)
    if (inputFFTEnabled && inputFFTProcessor != nullptr) {
        drawSpectrum(g, bar, inputFFTProcessor, 1.0f, juce::Colour(0x80FFFFFF));
    }
    
    // Remote FFTs: bright colored lines for each remote instance
    if (remoteFFTEnabled && !remoteSpectrums.empty()) {
        // Use different colors for each remote instance (cycle through hues)
        const float hueStep = 360.0f / 12.0f; // 12 distinct colors
        for (size_t i = 0; i < remoteSpectrums.size(); ++i) {
            float hue = std::fmod(static_cast<float>(remoteSpectrums[i].instanceID % 12) * hueStep, 360.0f);
            juce::Colour remoteColour = juce::Colour::fromHSV(hue / 360.0f, 0.9f, 1.0f, 0.85f);
            drawRemoteSpectrum(g, bar, remoteSpectrums[i], 2.5f, remoteColour);
        }
    }

    // Draw reference grid lines (behind everything)
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    static constexpr float gridValues[] = {-12.0f, -6.0f, 0.0f, 6.0f, 12.0f};
    for (float gridDB : gridValues) {
        int gridY = static_cast<int>(std::round(gainToY(gridDB)));
        float dashPattern[] = {3.0f, 3.0f};
        g.drawDashedLine(
            juce::Line<float>(static_cast<float>(bar.getX()), static_cast<float>(gridY),
                              static_cast<float>(bar.getRight()), static_cast<float>(gridY)),
            dashPattern, 2, 1.0f);
    }

    // Draw coloured band regions with semi-transparent fill
    for (size_t band = 0; band < NUM_BANDS; ++band) {
        float x0 = (band == 0) ? static_cast<float>(bar.getX()) : freqToX(freqs[band - 1]);
        float x1 =
            (band == NUM_BANDS - 1) ? static_cast<float>(bar.getRight()) : freqToX(freqs[band]);

        juce::Rectangle<float> bandRect(x0, static_cast<float>(bar.getY()), x1 - x0,
                                        static_cast<float>(bar.getHeight()));

        // Base color with semi-transparency
        juce::Colour bandColour = juce::Colour(BAND_COLOURS[band]).withAlpha(0.6f);

        // If dragging this band's gain line, highlight the entire region
        if (dragGainBandIndex == static_cast<int>(band)) {
            bandColour = bandColour.brighter(0.3f).withAlpha(0.7f);
        } else if (hoverGainBandIndex == static_cast<int>(band)) {
            bandColour = bandColour.brighter(0.15f);
        }

        g.setColour(bandColour);
        g.fillRect(bandRect);

        // Band name label (centred in region, only if wide enough)
        if (bandRect.getWidth() > 30.0f) {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));

            // Position label at top of band
            auto labelRect = bandRect.withHeight(20.0f);
            g.drawText(BAND_NAMES[band], labelRect.toNearestInt(), juce::Justification::centred,
                       false);
        }
    }

    // Draw horizontal gain lines (one per band)
    for (size_t band = 0; band < NUM_BANDS; ++band) {
        float x0 = (band == 0) ? static_cast<float>(bar.getX()) : freqToX(freqs[band - 1]);
        float x1 =
            (band == NUM_BANDS - 1) ? static_cast<float>(bar.getRight()) : freqToX(freqs[band]);

        float lineY = gainToY(bandGainsDB[band]);

        bool dragging = (dragGainBandIndex == static_cast<int>(band));
        bool hovering = (hoverGainBandIndex == static_cast<int>(band));
        bool active = dragging || hovering;

        // Line color and thickness
        juce::Colour lineColour = active ? juce::Colours::cyan : juce::Colour(BAND_COLOURS[band]);
        float thickness = active ? 3.0f : 2.0f;

        g.setColour(lineColour);
        g.drawLine(x0, lineY, x1, lineY, thickness);

        // Draw gain value label
        juce::String gainText;
        if (bandGainsDB[band] > 0.0f)
            gainText = "+" + juce::String(bandGainsDB[band], 1) + "dB";
        else
            gainText = juce::String(bandGainsDB[band], 1) + "dB";
        float fontSize = active ? 11.0f : 10.0f;
        int styleFlags = active ? juce::Font::bold : juce::Font::plain;
        juce::Font font = juce::Font(juce::FontOptions(fontSize, styleFlags));
        g.setFont(font);
        g.setColour(active ? juce::Colours::white : juce::Colours::white.withAlpha(0.8f));

        // Position label to the right of the line (or left if near edge)
        float labelX = x1 - 50.0f;
        if (labelX < x0 + 5.0f)
            labelX = x0 + 5.0f;

        g.drawText(gainText, static_cast<int>(labelX), static_cast<int>(lineY - 8), 45, 16,
                   juce::Justification::centredLeft, false);
    }

    // Draw divider lines last (on top)
    for (size_t i = 0; i < NUM_FREQS; ++i) {
        int dx = dividerXForIndex(i);

        // Glow when hovered or dragged
        bool dragging = (dragIndex == static_cast<int>(i));
        bool hovering = (hoverIndex == static_cast<int>(i));
        bool active = dragging || hovering;

        g.setColour(active ? juce::Colours::cyan : juce::Colours::white.withAlpha(0.8f));
        g.drawVerticalLine(dx, static_cast<float>(bar.getY()), static_cast<float>(bar.getBottom()));

        // Slightly thicker handle zone
        if (active) {
            g.setColour(juce::Colours::cyan.withAlpha(dragging ? 0.5f : 0.3f));
            g.fillRect(dx - 2, bar.getY(), 5, bar.getHeight());
        }
    }

    // Draw frequency axis ticks at bottom of bar
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));

    static const float tickFreqs[] = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    static const char* tickLabels[] = {"20", "50", "100", "200", "500",
                                       "1k", "2k", "5k",  "10k", "20k"};

    for (int t = 0; t < 10; ++t) {
        int tx = static_cast<int>(std::round(freqToX(tickFreqs[t])));
        g.drawVerticalLine(tx, static_cast<float>(bar.getBottom() - 4),
                           static_cast<float>(bar.getBottom()));
        g.drawText(tickLabels[t], tx - 15, bar.getBottom() - 14, 30, 12,
                   juce::Justification::centred, false);
    }
}

// ============================================================================
// Resized - position text boxes below dividers
// ============================================================================

void CrossoverFrequencyBar::resized() {
    static constexpr int boxW = 60;
    static constexpr int boxH = 22;

    auto textArea = getTextBoxArea();
    int textY = textArea.getY() + (textArea.getHeight() - boxH) / 2;

    for (size_t i = 0; i < NUM_FREQS; ++i) {
        if (freqLabels[i]) {
            int cx = dividerXForIndex(i);
            int bx = juce::jlimit(0, getWidth() - boxW, cx - boxW / 2);
            freqLabels[i]->setBounds(bx, textY, boxW, boxH);
        }
    }
}

// ============================================================================
// Mouse interaction for divider dragging
// ============================================================================

void CrossoverFrequencyBar::mouseDown(const juce::MouseEvent& e) {
    // Check gain line hit test BEFORE divider hit test (priority)
    int gainHit = hitTestGainLine(e.x, e.y);
    if (gainHit >= 0) {
        dragGainBandIndex = gainHit;
        repaint();
        return;
    }

    // Check divider hit test
    dragIndex = hitTestDivider(e.x);
    if (dragIndex >= 0)
        repaint();
}

void CrossoverFrequencyBar::mouseDrag(const juce::MouseEvent& e) {
    // Handle gain line dragging
    if (dragGainBandIndex >= 0) {
        float rawGain = yToGain(static_cast<float>(e.y));

        // Always quantize to 0.1 dB to match parameter resolution
        rawGain = std::round(rawGain * 10.0f) / 10.0f;

        // Apply snap-to-grid (for common values like 0, ±6, ±12 dB)
        float snappedGain = applySnapToGrid(rawGain);

        // Clamp to valid range
        snappedGain = juce::jlimit(MIN_GAIN_DB, MAX_GAIN_DB, snappedGain);

        bandGainsDB[static_cast<size_t>(dragGainBandIndex)] = snappedGain;
        pushGainToParam(static_cast<size_t>(dragGainBandIndex), snappedGain);

        repaint();
        return;
    }

    // Handle divider dragging
    if (dragIndex < 0)
        return;

    float rawFreq = xToFreq(static_cast<float>(e.x));
    float clamped = clampFreqForIndex(static_cast<size_t>(dragIndex), rawFreq);

    freqs[static_cast<size_t>(dragIndex)] = clamped;
    updateTextBoxFromFreq(static_cast<size_t>(dragIndex));
    pushFreqToParam(static_cast<size_t>(dragIndex), clamped);

    // Reposition text box under divider
    resized();
    repaint();
}

void CrossoverFrequencyBar::mouseUp(const juce::MouseEvent&) {
    dragIndex = -1;
    dragGainBandIndex = -1;
    repaint();
}

void CrossoverFrequencyBar::mouseMove(const juce::MouseEvent& e) {
    // Check gain line hover first (priority)
    int gainHit = hitTestGainLine(e.x, e.y);
    if (gainHit >= 0) {
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        if (gainHit != hoverGainBandIndex) {
            hoverGainBandIndex = gainHit;
            hoverIndex = -1; // clear divider hover
            repaint();
        }
        return;
    }

    // Check divider hover
    int hit = hitTestDivider(e.x);
    setMouseCursor(hit >= 0 ? juce::MouseCursor::LeftRightResizeCursor
                            : juce::MouseCursor::NormalCursor);

    if (hit != hoverIndex || hoverGainBandIndex >= 0) {
        hoverIndex = hit;
        hoverGainBandIndex = -1; // clear gain hover
        repaint();
    }
}

void CrossoverFrequencyBar::mouseExit(const juce::MouseEvent&) {
    if (hoverIndex >= 0 || hoverGainBandIndex >= 0) {
        hoverIndex = -1;
        hoverGainBandIndex = -1;
        repaint();
    }
}

void CrossoverFrequencyBar::mouseDoubleClick(const juce::MouseEvent& e) {
    // Check if double-clicking on a gain line
    int gainHit = hitTestGainLine(e.x, e.y);
    if (gainHit >= 0) {
        // Reset gain to 0 dB
        bandGainsDB[static_cast<size_t>(gainHit)] = 0.0f;
        pushGainToParam(static_cast<size_t>(gainHit), 0.0f);
        repaint();
    }
}

void CrossoverFrequencyBar::drawSpectrum(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                                         FFTProcessor* fftProcessor, float lineWidth, juce::Colour colour) {
    if (fftProcessor == nullptr)
        return;

    const float* magnitudes = fftProcessor->getMagnitudeSpectrum();
    const int numBins = fftProcessor->getNumBins();
    const float sampleRate = static_cast<float>(processorRef.getSampleRate());

    if (numBins <= 0 || sampleRate <= 0.0f)
        return;

    // Draw spectrum with logarithmic frequency scale
    // Horizontal bar: frequency maps to X axis, magnitude maps to Y axis
    const float minFreq = 20.0f;
    const float maxFreq = sampleRate * 0.5f; // Nyquist
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);

    const int width = bounds.getWidth();
    const int height = bounds.getHeight();

    // Build spectrum path with cubic interpolation for smooth curves
    juce::Path spectrumPath;
    bool firstPoint = true;

    // Sample points across the width
    for (int x = 0; x < width; ++x) {
        // Map X position to frequency (logarithmic)
        const float proportion = static_cast<float>(x) / static_cast<float>(width);
        const float logFreq = logMin + proportion * (logMax - logMin);
        const float freq = std::pow(10.0f, logFreq);

        // Calculate exact fractional bin position (for interpolation)
        const float exactBin = freq * static_cast<float>(fftProcessor->getFFTSize()) / sampleRate;
        const int bin0 = static_cast<int>(std::floor(exactBin));
        const int bin1 = bin0 + 1;
        const float binFraction = exactBin - static_cast<float>(bin0);

        // Interpolate magnitude between adjacent bins
        float magnitude = 0.0f;
        if (bin0 >= 0 && bin1 < numBins) {
            const float mag0 = magnitudes[bin0];
            const float mag1 = magnitudes[bin1];
            magnitude = mag0 + binFraction * (mag1 - mag0);
        } else if (bin0 >= 0 && bin0 < numBins) {
            magnitude = magnitudes[bin0];
        } else {
            continue;
        }

        // Convert to dB (with floor to avoid log(0))
        magnitude = juce::jmax(magnitude, 1e-9f);
        const float dB = 20.0f * std::log10(magnitude);

        // Map dB to vertical position
        // Range [-80, 0] dB gives 80 dB of dynamic range, enough for broadband audio
        // where per-bin magnitudes are naturally much lower than a pure sine wave
        const float dbMin = -80.0f;
        const float dbMax = 0.0f;
        const float normalizedDB = juce::jlimit(0.0f, 1.0f, (dB - dbMin) / (dbMax - dbMin));
        const float yPos = bounds.getY() + (1.0f - normalizedDB) * static_cast<float>(height);
        const float xPos = static_cast<float>(bounds.getX() + x);

        if (firstPoint) {
            spectrumPath.startNewSubPath(xPos, yPos);
            firstPoint = false;
        } else {
            spectrumPath.lineTo(xPos, yPos);
        }
    }

    // Smooth the path using rounded corners for natural spectrum appearance
    if (!firstPoint) {
        juce::Path smoothPath = spectrumPath.createPathWithRoundedCorners(1.5f);
        g.setColour(colour);
        g.strokePath(smoothPath, juce::PathStrokeType(lineWidth));
    }
}

void CrossoverFrequencyBar::drawRemoteSpectrum(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                                               const SpectrumBroadcaster::RemoteSpectrum& spectrum,
                                               float lineWidth, juce::Colour colour) {
    if (spectrum.magnitudes.empty()) {
        return;
    }

    const int numBins = static_cast<int>(spectrum.magnitudes.size());
    const float sampleRate = spectrum.sampleRate;

    if (numBins <= 0 || sampleRate <= 0.0f) {
        return;
    }

    // Draw spectrum with logarithmic frequency scale
    const float minFreq = 20.0f;
    const float maxFreq = sampleRate * 0.5f; // Nyquist
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);

    const int width = bounds.getWidth();
    const int height = bounds.getHeight();

    // Build spectrum path
    juce::Path spectrumPath;
    bool firstPoint = true;

    // Sample points across the width
    for (int x = 0; x < width; ++x) {
        // Map X position to frequency (logarithmic)
        const float proportion = static_cast<float>(x) / static_cast<float>(width);
        const float logFreq = logMin + proportion * (logMax - logMin);
        const float freq = std::pow(10.0f, logFreq);

        // Calculate exact fractional bin position (for interpolation)
        // Remote spectrums are compressed to MAX_SPECTRUM_BINS
        const float exactBin = (freq / (sampleRate * 0.5f)) * static_cast<float>(numBins);
        const int bin0 = static_cast<int>(std::floor(exactBin));
        const int bin1 = bin0 + 1;
        const float binFraction = exactBin - static_cast<float>(bin0);

        // Interpolate magnitude between adjacent bins
        float magnitude = 0.0f;
        if (bin0 >= 0 && bin1 < numBins) {
            const float mag0 = spectrum.magnitudes[bin0];
            const float mag1 = spectrum.magnitudes[bin1];
            magnitude = mag0 + binFraction * (mag1 - mag0);
        } else if (bin0 >= 0 && bin0 < numBins) {
            magnitude = spectrum.magnitudes[bin0];
        } else {
            continue;
        }

        // Convert to dB (remote magnitudes are already dequantized 0-1)
        magnitude = juce::jmax(magnitude, 1e-9f);
        const float dB = 20.0f * std::log10(magnitude);

        // Map dB to vertical position
        const float dbMin = -80.0f;
        const float dbMax = 0.0f;
        const float normalizedDB = juce::jlimit(0.0f, 1.0f, (dB - dbMin) / (dbMax - dbMin));
        const float yPos = bounds.getY() + (1.0f - normalizedDB) * static_cast<float>(height);
        const float xPos = static_cast<float>(bounds.getX() + x);

        if (firstPoint) {
            spectrumPath.startNewSubPath(xPos, yPos);
            firstPoint = false;
        } else {
            spectrumPath.lineTo(xPos, yPos);
        }
    }

    // Smooth and draw the path
    if (!firstPoint) {
        juce::Path smoothPath = spectrumPath.createPathWithRoundedCorners(1.5f);
        g.setColour(colour);
        g.strokePath(smoothPath, juce::PathStrokeType(lineWidth));
    }
}
