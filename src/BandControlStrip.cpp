#include "BandControlStrip.h"
#include "PluginProcessor.h"

BandControlStrip::BandControlStrip(PhuSplitterAudioProcessor& processor)
    : processorRef(processor) {
    // Create solo and mute buttons for each band
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        // Create solo button
        soloButtons[i] = std::make_unique<juce::TextButton>("S");
        soloButtons[i]->setClickingTogglesState(true);
        soloButtons[i]->onClick = [this, i]() { onSoloButtonClicked(i); };
        addAndMakeVisible(*soloButtons[i]);

        // Create mute button
        muteButtons[i] = std::make_unique<juce::TextButton>("M");
        muteButtons[i]->setClickingTogglesState(true);
        muteButtons[i]->onClick = [this, i]() { onMuteButtonClicked(i); };
        addAndMakeVisible(*muteButtons[i]);
    }

    // Update initial button states
    updateButtonStates();

    // Start timer for parameter synchronization (60 Hz refresh rate)
    startTimer(16);
}

BandControlStrip::~BandControlStrip() {
    stopTimer();
}

void BandControlStrip::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::darkgrey.darker());
}

void BandControlStrip::resized() {
    auto area = getLocalBounds();
    const int buttonHeight = area.getHeight() / static_cast<int>(NUM_BANDS);

    for (size_t i = 0; i < NUM_BANDS; ++i) {
        auto rowArea = area.removeFromTop(buttonHeight);

        // Solo button on the left
        auto soloArea = rowArea.removeFromLeft(SOLO_BUTTON_WIDTH);
        soloButtons[i]->setBounds(soloArea);

        // Gap between columns
        rowArea.removeFromLeft(COLUMN_GAP);

        // Mute button on the right
        auto muteArea = rowArea.removeFromLeft(MUTE_BUTTON_WIDTH);
        muteButtons[i]->setBounds(muteArea);
    }
}

void BandControlStrip::onSoloButtonClicked(size_t bandIndex) {
    if (bandIndex < NUM_BANDS) {
        bool newState = soloButtons[bandIndex]->getToggleState();
        processorRef.setBandSolo(bandIndex, newState);
    }
}

void BandControlStrip::onMuteButtonClicked(size_t bandIndex) {
    if (bandIndex < NUM_BANDS) {
        bool newState = muteButtons[bandIndex]->getToggleState();
        processorRef.setBandMute(bandIndex, newState);
    }
}

void BandControlStrip::updateButtonStates() {
    auto soloStates = processorRef.getBandSoloStates();
    auto muteStates = processorRef.getBandMuteStates();

    for (size_t i = 0; i < NUM_BANDS; ++i) {
        // Update solo button
        bool isSoloed = soloStates[i];
        soloButtons[i]->setToggleState(isSoloed, juce::dontSendNotification);

        // Style solo button — active solo is orange/yellow, inactive is always dark grey
        // Note: must set both buttonColourId (off) and buttonOnColourId (on/toggled)
        if (isSoloed) {
            // Active solo: orange/yellow background
            soloButtons[i]->setColour(juce::TextButton::buttonColourId,
                                      juce::Colour(0xFFE8A000)); // Orange/yellow
            soloButtons[i]->setColour(juce::TextButton::buttonOnColourId,
                                      juce::Colour(0xFFE8A000)); // Orange/yellow
            soloButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            soloButtons[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        } else {
            // Inactive solo: dark grey — same look regardless of whether other bands are soloed
            soloButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
            soloButtons[i]->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF333333));
            soloButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
            soloButtons[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::lightgrey);
        }

        // Update mute button
        bool isMuted = muteStates[i];
        muteButtons[i]->setToggleState(isMuted, juce::dontSendNotification);

        // Style mute button
        if (isMuted) {
            // Active mute: red background
            muteButtons[i]->setColour(juce::TextButton::buttonColourId,
                                      juce::Colour(0xFFCC0000)); // Red
            muteButtons[i]->setColour(juce::TextButton::buttonOnColourId,
                                      juce::Colour(0xFFCC0000)); // Red
            muteButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            muteButtons[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        } else {
            // Inactive mute: dark grey
            muteButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
            muteButtons[i]->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF333333));
            muteButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
            muteButtons[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::lightgrey);
        }
    }
}

void BandControlStrip::timerCallback() {
    updateButtonStates();
}
