# Contributing to phu-splitter

Contributions are welcome.

1. Fork and branch from `main`
2. Follow existing C++17/JUCE code style
3. No memory allocation, system calls, or locks on the audio thread
4. Verify the project builds and passes pluginval before opening a PR

**Bug reports** — please include DAW name/version, OS, and reproduction steps in a [GitHub Issue](https://github.com/huberp/phu-splitter/issues).

---

## Implementation Details

- **Linkwitz-Riley filters**: 8th order (48 dB/octave) crossovers implemented using cascaded biquad filters in Direct Form I
- **Phase coherent**: Allpass compensation ensures flat magnitude response when all bands are summed
- **Multi-output architecture**: 1 stereo input → 7 stereo output buses
- **Real-time safe**: Lock-free logging system for editor updates from audio thread
- **Note-to-frequency conversion**: Header-only `NoteToFreq` utility in `lib/`

## Project Structure

| Path | Description |
|------|-------------|
| `src/LinkwitzRileyFilter.h` | Core DSP — biquad filters, LR crossovers, MultiBandN |
| `src/PluginProcessor.cpp/.h` | Audio processor with APVTS parameter management |
| `src/PluginEditor.cpp/.h` | Plugin editor UI layout |
| `src/CrossoverFrequencyBar.cpp/.h` | Draggable frequency bar with text input |
| `src/PresetManager.cpp/.h` | Preset storage engine |
| `src/PresetStrip.cpp/.h` | Inline preset navigation UI |
| `lib/NoteToFreq.h` | Musical note name → frequency converter |
| `lib/` | Event system library (see `lib/README.md`) |
