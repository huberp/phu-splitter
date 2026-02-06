#include "PresetManager.h"
#include "PluginProcessor.h"

// ============================================================================
// Construction
// ============================================================================

PresetManager::PresetManager(PhuSplitterAudioProcessor& processor)
    : processorRef(processor)
{
    // Ensure presets directory exists
    getPresetsDirectory().createDirectory();
    scanPresets();
    currentPresetName = INIT_PRESET_NAME;
}

// ============================================================================
// Directory & file helpers
// ============================================================================

juce::File PresetManager::getPresetsDirectory() const
{
    auto appDataDir = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory);
    return appDataDir.getChildFile("PhuSplitter").getChildFile("Presets");
}

juce::File PresetManager::getPresetFile(const juce::String& name) const
{
    juce::String safeName = juce::File::createLegalFileName(name);
    if (safeName.isEmpty())
        safeName = "Preset";
    return getPresetsDirectory().getChildFile(safeName + PRESET_FILE_EXTENSION);
}

void PresetManager::writePresetFile(const Preset& preset) const
{
    auto xml = std::make_unique<juce::XmlElement>("Preset");
    xml->setAttribute("name", preset.name);

    for (size_t i = 0; i < preset.frequencies.size(); ++i)
    {
        auto* freqEl = xml->createNewChildElement("Frequency");
        freqEl->setAttribute("index", static_cast<int>(i));
        freqEl->setAttribute("value", static_cast<double>(preset.frequencies[i]));
    }

    auto file = getPresetFile(preset.name);
    xml->writeTo(file);
}

std::optional<Preset> PresetManager::readPresetFile(const juce::File& file) const
{
    auto xml = juce::XmlDocument::parse(file);
    if (!xml || !xml->hasTagName("Preset"))
        return std::nullopt;

    Preset preset;
    preset.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());

    // Read frequencies — default to Init values if missing
    auto defaults = PhuSplitterAudioProcessor::DEFAULT_CROSSOVER_FREQS;
    for (size_t i = 0; i < preset.frequencies.size(); ++i)
        preset.frequencies[i] = defaults[i];

    for (auto* child : xml->getChildIterator())
    {
        if (child->hasTagName("Frequency"))
        {
            int idx = child->getIntAttribute("index", -1);
            if (idx >= 0 && idx < static_cast<int>(preset.frequencies.size()))
                preset.frequencies[static_cast<size_t>(idx)] =
                    static_cast<float>(child->getDoubleAttribute("value", defaults[static_cast<size_t>(idx)]));
        }
    }

    return preset;
}

// ============================================================================
// Scanning
// ============================================================================

void PresetManager::scanPresets()
{
    presetNames.clear();
    presetNames.add(INIT_PRESET_NAME);

    auto dir = getPresetsDirectory();
    if (!dir.isDirectory())
        return;

    auto files = dir.findChildFiles(juce::File::findFiles, false, juce::String("*") + PRESET_FILE_EXTENSION);
    files.sort();

    for (auto& file : files)
    {
        auto name = file.getFileNameWithoutExtension();
        if (name.compareIgnoreCase(INIT_PRESET_NAME) != 0)
            presetNames.add(name);
    }
}

// ============================================================================
// Queries
// ============================================================================

juce::StringArray PresetManager::getPresetNames() const
{
    return presetNames;
}

int PresetManager::getCurrentPresetIndex() const
{
    return presetNames.indexOf(currentPresetName);
}

int PresetManager::getNumPresets() const
{
    return presetNames.size();
}

// ============================================================================
// Apply / Read processor frequencies
// ============================================================================

void PresetManager::applyPreset(const Preset& preset)
{
    for (size_t i = 0; i < preset.frequencies.size(); ++i)
        processorRef.setCrossoverFrequency(i, preset.frequencies[i]);
}

std::array<float, 6> PresetManager::readProcessorFrequencies() const
{
    return processorRef.getCrossoverFrequencies();
}

// ============================================================================
// Actions
// ============================================================================

void PresetManager::loadInit()
{
    Preset init;
    init.name = INIT_PRESET_NAME;
    init.frequencies = PhuSplitterAudioProcessor::DEFAULT_CROSSOVER_FREQS;
    applyPreset(init);
    currentPresetName = INIT_PRESET_NAME;

    if (onPresetsChanged)
        onPresetsChanged();
}

void PresetManager::loadPreset(const juce::String& name)
{
    if (name.compareIgnoreCase(INIT_PRESET_NAME) == 0)
    {
        loadInit();
        return;
    }

    auto file = getPresetFile(name);
    if (!file.existsAsFile())
        return;

    auto preset = readPresetFile(file);
    if (preset)
    {
        applyPreset(*preset);
        currentPresetName = preset->name;

        if (onPresetsChanged)
            onPresetsChanged();
    }
}

void PresetManager::loadPresetByIndex(int index)
{
    if (index >= 0 && index < presetNames.size())
        loadPreset(presetNames[index]);
}

void PresetManager::savePreset(const juce::String& name)
{
    if (name.isEmpty() || name.compareIgnoreCase(INIT_PRESET_NAME) == 0)
        return;

    Preset preset;
    preset.name = name;
    preset.frequencies = readProcessorFrequencies();
    writePresetFile(preset);

    currentPresetName = name;
    scanPresets();

    if (onPresetsChanged)
        onPresetsChanged();
}

void PresetManager::deletePreset(const juce::String& name)
{
    if (name.compareIgnoreCase(INIT_PRESET_NAME) == 0)
        return;

    auto file = getPresetFile(name);
    if (file.existsAsFile())
        file.deleteFile();

    if (currentPresetName == name)
        currentPresetName = INIT_PRESET_NAME;

    scanPresets();

    if (onPresetsChanged)
        onPresetsChanged();
}

void PresetManager::renamePreset(const juce::String& oldName, const juce::String& newName)
{
    if (oldName.compareIgnoreCase(INIT_PRESET_NAME) == 0 || newName.isEmpty())
        return;
    if (newName.compareIgnoreCase(INIT_PRESET_NAME) == 0)
        return;

    auto oldFile = getPresetFile(oldName);
    if (!oldFile.existsAsFile())
        return;

    auto preset = readPresetFile(oldFile);
    if (!preset)
        return;

    // Write under new name, delete old file
    preset->name = newName;
    writePresetFile(*preset);
    oldFile.deleteFile();

    if (currentPresetName == oldName)
        currentPresetName = newName;

    scanPresets();

    if (onPresetsChanged)
        onPresetsChanged();
}

void PresetManager::loadNextPreset()
{
    int idx = getCurrentPresetIndex();
    int next = (idx + 1) % getNumPresets();
    loadPresetByIndex(next);
}

void PresetManager::loadPreviousPreset()
{
    int idx = getCurrentPresetIndex();
    int prev = (idx - 1 + getNumPresets()) % getNumPresets();
    loadPresetByIndex(prev);
}
