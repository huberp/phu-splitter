#include "CrossoverFrequencyBar.h"
#include "PluginProcessor.h"
#include "NoteToFreq.h"

// ============================================================================
// Construction / Destruction
// ============================================================================

CrossoverFrequencyBar::CrossoverFrequencyBar(PhuSplitterAudioProcessor& processor)
    : processorRef(processor)
{
    // Pull initial frequencies from processor parameters
    auto initFreqs = processorRef.getCrossoverFrequencies();
    for (size_t i = 0; i < NUM_FREQS; ++i)
        freqs[i] = initFreqs[i];

    createTextBoxes();

    // Poll parameter changes at 15 Hz (for DAW automation reflection)
    startTimerHz(15);
}

CrossoverFrequencyBar::~CrossoverFrequencyBar()
{
    stopTimer();
}

// ============================================================================
// Coordinate conversion  (logarithmic frequency axis)
// ============================================================================

float CrossoverFrequencyBar::freqToX(float freq) const
{
    auto bar = getBarArea().toFloat();
    if (bar.getWidth() <= 0.0f) return bar.getX();

    float logMin = std::log(MIN_FREQ);
    float logMax = std::log(MAX_FREQ);
    float norm   = (std::log(freq) - logMin) / (logMax - logMin);
    return bar.getX() + norm * bar.getWidth();
}

float CrossoverFrequencyBar::xToFreq(float x) const
{
    auto bar = getBarArea().toFloat();
    if (bar.getWidth() <= 0.0f) return MIN_FREQ;

    float norm   = (x - bar.getX()) / bar.getWidth();
    norm = juce::jlimit(0.0f, 1.0f, norm);
    float logMin = std::log(MIN_FREQ);
    float logMax = std::log(MAX_FREQ);
    return std::exp(logMin + norm * (logMax - logMin));
}

// ============================================================================
// Frequency validation - keep dividers in order with min gap
// ============================================================================

float CrossoverFrequencyBar::clampFreqForIndex(size_t index, float freq) const
{
    float lo = MIN_FREQ;
    float hi = MAX_FREQ;

    if (index == 0)
    {
        // First divider: allow down to MIN_FREQ, but keep a minimum ratio to the next divider
        lo = MIN_FREQ;
        if (NUM_FREQS > 1)
            hi = freqs[1] / MIN_FREQ_RATIO;
        else
            hi = MAX_FREQ;
    }
    else if (index == NUM_FREQS - 1)
    {
        // Last divider: allow up to MAX_FREQ, but keep a minimum ratio to the previous divider
        lo = freqs[index - 1] * MIN_FREQ_RATIO;
        hi = MAX_FREQ;
    }
    else
    {
        // Middle dividers: constrained between neighboring dividers
        lo = freqs[index - 1] * MIN_FREQ_RATIO;
        hi = freqs[index + 1] / MIN_FREQ_RATIO;
    }
    return juce::jlimit(lo, hi, freq);
}

// ============================================================================
// Text boxes
// ============================================================================

static juce::String formatFreq(float hz)
{
    if (hz >= 1000.0f)
        return juce::String(hz / 1000.0f, 2) + "k";
    return juce::String(static_cast<int>(std::round(hz)));
}

static float parseFreq(const juce::String& text)
{
    juce::String trimmed = text.trim();
    std::string str = trimmed.toStdString();

    // Try note name first (e.g. "C#3", "E#1", "Ab4")
    if (NoteToFreq::looksLikeNoteName(str))
    {
        auto freq = NoteToFreq::toFrequency(str);
        if (freq.has_value())
            return static_cast<float>(freq.value());
    }

    // Fall through to numeric parsing
    juce::String lower = trimmed.toLowerCase();

    // Handle "k" suffix for kHz
    if (lower.endsWithChar('k'))
    {
        float val = lower.dropLastCharacters(1).getFloatValue();
        return val * 1000.0f;
    }
    if (lower.endsWith("khz"))
    {
        float val = lower.dropLastCharacters(3).getFloatValue();
        return val * 1000.0f;
    }
    if (lower.endsWith("hz"))
    {
        return lower.dropLastCharacters(2).getFloatValue();
    }

    return lower.getFloatValue();
}

void CrossoverFrequencyBar::createTextBoxes()
{
    for (size_t i = 0; i < NUM_FREQS; ++i)
    {
        auto label = std::make_unique<juce::Label>();
        label->setEditable(false, true, false); // not editable by single click, but by double-click
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::Font(12.0f));
        label->setColour(juce::Label::textColourId, juce::Colours::white);
        label->setColour(juce::Label::backgroundColourId, juce::Colour(0xFF333333));
        label->setColour(juce::Label::outlineColourId, juce::Colour(0xFF666666));
        label->setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
        label->setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xFF222222));
        label->setColour(juce::Label::outlineWhenEditingColourId, juce::Colours::cyan);
        
        updateTextBoxFromFreq(i); // set initial text after label is created... handled below

        const size_t idx = i;
        label->onTextChange = [this, idx]()
        {
            onTextBoxReturnKey(idx);
        };

        addAndMakeVisible(label.get());
        freqLabels[i] = std::move(label);
        
        // Now set initial text
        updateTextBoxFromFreq(i);
    }
}

void CrossoverFrequencyBar::updateTextBoxFromFreq(size_t index)
{
    if (freqLabels[index])
        freqLabels[index]->setText(formatFreq(freqs[index]), juce::dontSendNotification);
}

void CrossoverFrequencyBar::onTextBoxReturnKey(size_t index)
{
    if (!freqLabels[index]) return;
    
    float typed = parseFreq(freqLabels[index]->getText());
    if (typed <= 0.0f)
    {
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

void CrossoverFrequencyBar::pushFreqToParam(size_t index, float freq)
{
    processorRef.setCrossoverFrequency(index, freq);
}

void CrossoverFrequencyBar::pullFreqsFromParams()
{
    auto paramFreqs = processorRef.getCrossoverFrequencies();
    bool changed = false;
    for (size_t i = 0; i < NUM_FREQS; ++i)
    {
        if (std::abs(paramFreqs[i] - freqs[i]) > 0.01f)
        {
            freqs[i] = paramFreqs[i];
            updateTextBoxFromFreq(i);
            changed = true;
        }
    }
    if (changed)
    {
        resized();  // reposition text boxes under new divider locations
        repaint();
    }
}

// ============================================================================
// Timer - sync with DAW automation
// ============================================================================

void CrossoverFrequencyBar::timerCallback()
{
    if (dragIndex < 0)  // don't overwrite while user is dragging
        pullFreqsFromParams();
}

// ============================================================================
// Layout helpers
// ============================================================================

juce::Rectangle<int> CrossoverFrequencyBar::getBarArea() const
{
    auto area = getLocalBounds();
    // Top part is the coloured bar, bottom 28px are text boxes
    return area.withTrimmedBottom(32);
}

juce::Rectangle<int> CrossoverFrequencyBar::getTextBoxArea() const
{
    auto area = getLocalBounds();
    return area.removeFromBottom(28);
}

int CrossoverFrequencyBar::dividerXForIndex(size_t index) const
{
    return static_cast<int>(std::round(freqToX(freqs[index])));
}

int CrossoverFrequencyBar::hitTestDivider(int x) const
{
    int halfZone = getDividerHitZoneHalfWidth();
    for (size_t i = 0; i < NUM_FREQS; ++i)
    {
        int dx = dividerXForIndex(i);
        if (std::abs(x - dx) <= halfZone)
            return static_cast<int>(i);
    }
    return -1;
}

// ============================================================================
// Paint
// ============================================================================

void CrossoverFrequencyBar::paint(juce::Graphics& g)
{
    auto bar = getBarArea();
    
    // Draw coloured band regions
    for (size_t band = 0; band < NUM_BANDS; ++band)
    {
        float x0 = (band == 0)            ? static_cast<float>(bar.getX()) : freqToX(freqs[band - 1]);
        float x1 = (band == NUM_BANDS - 1) ? static_cast<float>(bar.getRight()) : freqToX(freqs[band]);
        
        juce::Rectangle<float> bandRect(x0, static_cast<float>(bar.getY()),
                                         x1 - x0, static_cast<float>(bar.getHeight()));
        
        g.setColour(juce::Colour(BAND_COLOURS[band]));
        g.fillRect(bandRect);
        
        // Band name label (centred in region, only if wide enough)
        if (bandRect.getWidth() > 30.0f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText(BAND_NAMES[band], bandRect.toNearestInt(), juce::Justification::centred, false);
        }
    }
    
    // Draw divider lines
    for (size_t i = 0; i < NUM_FREQS; ++i)
    {
        int dx = dividerXForIndex(i);
        
        // Glow when hovered or dragged
        bool active = (dragIndex == static_cast<int>(i));
        g.setColour(active ? juce::Colours::cyan : juce::Colours::white.withAlpha(0.8f));
        g.drawVerticalLine(dx, static_cast<float>(bar.getY()), static_cast<float>(bar.getBottom()));
        
        // Slightly thicker handle zone
        if (active)
        {
            g.setColour(juce::Colours::cyan.withAlpha(0.3f));
            g.fillRect(dx - 2, bar.getY(), 5, bar.getHeight());
        }
    }
    
    // Draw frequency axis ticks at bottom of bar
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::Font(9.0f));
    
    static const float tickFreqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    static const char* tickLabels[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    
    for (int t = 0; t < 10; ++t)
    {
        int tx = static_cast<int>(std::round(freqToX(tickFreqs[t])));
        g.drawVerticalLine(tx, static_cast<float>(bar.getBottom() - 4), static_cast<float>(bar.getBottom()));
        g.drawText(tickLabels[t], tx - 15, bar.getBottom() - 14, 30, 12, juce::Justification::centred, false);
    }
}

// ============================================================================
// Resized - position text boxes below dividers
// ============================================================================

void CrossoverFrequencyBar::resized()
{
    static constexpr int boxW = 60;
    static constexpr int boxH = 22;
    
    auto textArea = getTextBoxArea();
    int textY = textArea.getY() + (textArea.getHeight() - boxH) / 2;
    
    for (size_t i = 0; i < NUM_FREQS; ++i)
    {
        if (freqLabels[i])
        {
            int cx = dividerXForIndex(i);
            int bx = juce::jlimit(0, getWidth() - boxW, cx - boxW / 2);
            freqLabels[i]->setBounds(bx, textY, boxW, boxH);
        }
    }
}

// ============================================================================
// Mouse interaction for divider dragging
// ============================================================================

void CrossoverFrequencyBar::mouseDown(const juce::MouseEvent& e)
{
    dragIndex = hitTestDivider(e.x);
    if (dragIndex >= 0)
        repaint();
}

void CrossoverFrequencyBar::mouseDrag(const juce::MouseEvent& e)
{
    if (dragIndex < 0) return;
    
    float rawFreq = xToFreq(static_cast<float>(e.x));
    float clamped = clampFreqForIndex(static_cast<size_t>(dragIndex), rawFreq);
    
    freqs[static_cast<size_t>(dragIndex)] = clamped;
    updateTextBoxFromFreq(static_cast<size_t>(dragIndex));
    pushFreqToParam(static_cast<size_t>(dragIndex), clamped);
    
    // Reposition text box under divider
    resized();
    repaint();
}

void CrossoverFrequencyBar::mouseUp(const juce::MouseEvent&)
{
    dragIndex = -1;
    repaint();
}

void CrossoverFrequencyBar::mouseMove(const juce::MouseEvent& e)
{
    int hit = hitTestDivider(e.x);
    setMouseCursor(hit >= 0 ? juce::MouseCursor::LeftRightResizeCursor 
                            : juce::MouseCursor::NormalCursor);
}
