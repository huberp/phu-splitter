#include "PresetStrip.h"
#include "PluginProcessor.h"

// ============================================================================
// Construction
// ============================================================================

PresetStrip::PresetStrip(PhuSplitterAudioProcessor& processor)
    : processorRef(processor), presetManager(processor) {
    // -- Arrow buttons --
    prevButton.onClick = [this]() { presetManager.loadPreviousPreset(); };
    nextButton.onClick = [this]() { presetManager.loadNextPreset(); };
    addAndMakeVisible(prevButton);
    addAndMakeVisible(nextButton);

    // -- Preset name button (opens dropdown) --
    presetNameButton.onClick = [this]() { showPresetMenu(); };
    presetNameButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
    presetNameButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(presetNameButton);

    // -- Save / +New / Init --
    saveButton.onClick = [this]() {
        auto name = presetManager.getCurrentPresetName();
        if (name.isNotEmpty() && name.compareIgnoreCase("Init") != 0)
            presetManager.savePreset(name);
        else
            showSaveAsDialog(); // Can't overwrite Init; redirect to Save As
    };
    addAndMakeVisible(saveButton);

    newButton.onClick = [this]() { showSaveAsDialog(); };
    addAndMakeVisible(newButton);

    initButton.onClick = [this]() { presetManager.loadInit(); };
    addAndMakeVisible(initButton);

    // -- Callback when preset list / selection changes --
    presetManager.onPresetsChanged = [this]() { updatePresetNameDisplay(); };

    updatePresetNameDisplay();
}

// ============================================================================
// Layout
// ============================================================================

void PresetStrip::resized() {
    auto area = getLocalBounds().reduced(2, 0);

    static constexpr int arrowW = 28;
    static constexpr int btnW = 50;
    static constexpr int gap = 4;

    prevButton.setBounds(area.removeFromLeft(arrowW));
    area.removeFromLeft(gap);

    // Right-side buttons (from right to left)
    initButton.setBounds(area.removeFromRight(btnW));
    area.removeFromRight(gap);
    newButton.setBounds(area.removeFromRight(btnW));
    area.removeFromRight(gap);
    saveButton.setBounds(area.removeFromRight(btnW));
    area.removeFromRight(gap);

    nextButton.setBounds(area.removeFromRight(arrowW));
    area.removeFromRight(gap);

    // Remaining space is the preset name button
    presetNameButton.setBounds(area);
}

void PresetStrip::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF222222));
}

// ============================================================================
// Helpers
// ============================================================================

void PresetStrip::updatePresetNameDisplay() {
    auto name = presetManager.getCurrentPresetName();
    presetNameButton.setButtonText(name.isEmpty() ? "---" : name + "  \xE2\x96\xBC"); // ▼ triangle
}

// ============================================================================
// Dropdown menu
// ============================================================================

void PresetStrip::showPresetMenu() {
    juce::PopupMenu menu;
    auto names = presetManager.getPresetNames();
    auto current = presetManager.getCurrentPresetName();

    // Preset entries
    for (int i = 0; i < names.size(); ++i) {
        bool isCurrent = (names[i] == current);
        menu.addItem(1000 + i, names[i], true, isCurrent);
    }

    menu.addSeparator();

    // Management actions for the current preset
    bool canManage = current.compareIgnoreCase("Init") != 0 && current.isNotEmpty();

    menu.addItem(1, "Delete \"" + current + "\"", canManage);
    menu.addItem(2, "Rename...", canManage);

    juce::Component::SafePointer<PresetStrip> safeThis(this);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetNameButton),
                       [safeThis, names](int result) {
                           if (safeThis == nullptr)
                               return;

                           if (result == 0)
                               return; // dismissed

                           if (result == 1) {
                               // Delete current
                               safeThis->presetManager.deletePreset(
                                   safeThis->presetManager.getCurrentPresetName());
                           } else if (result == 2) {
                               // Rename
                               safeThis->showRenameDialog();
                           } else if (result >= 1000) {
                               int idx = result - 1000;
                               safeThis->presetManager.loadPresetByIndex(idx);
                           }
                       });
}

// ============================================================================
// Dialogs
// ============================================================================

void PresetStrip::showSaveAsDialog() {
    auto* alertWindow = new juce::AlertWindow(
        "Save Preset", "Enter a name for the new preset:", juce::MessageBoxIconType::NoIcon);
    alertWindow->addTextEditor("presetName", "", "Preset Name:");
    alertWindow->addButton("Save", 1);
    alertWindow->addButton("Cancel", 0);

    alertWindow->enterModalState(
        true, juce::ModalCallbackFunction::create([this, alertWindow](int result) {
            if (result == 1) {
                auto name = alertWindow->getTextEditorContents("presetName").trim();
                if (name.isNotEmpty() && name.compareIgnoreCase("Init") != 0)
                    presetManager.savePreset(name);
            }
            // AlertWindow deletes itself when it exits modal state
        }),
        true); // deleteWhenDismissed = true
}

void PresetStrip::showRenameDialog() {
    auto current = presetManager.getCurrentPresetName();
    if (current.compareIgnoreCase("Init") == 0)
        return;

    auto* alertWindow = new juce::AlertWindow(
        "Rename Preset",
        "Enter a new name for \"" + current + "\":", juce::MessageBoxIconType::NoIcon);
    alertWindow->addTextEditor("presetName", current, "Preset Name:");
    alertWindow->addButton("Rename", 1);
    alertWindow->addButton("Cancel", 0);

    alertWindow->enterModalState(
        true, juce::ModalCallbackFunction::create([this, alertWindow, current](int result) {
            if (result == 1) {
                auto newName = alertWindow->getTextEditorContents("presetName").trim();
                if (newName.isNotEmpty() && newName.compareIgnoreCase("Init") != 0 &&
                    newName != current)
                    presetManager.renamePreset(current, newName);
            }
        }),
        true);
}
